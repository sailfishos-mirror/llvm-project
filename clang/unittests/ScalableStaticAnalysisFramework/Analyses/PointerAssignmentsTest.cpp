//===- PointerAssignmentsTest.cpp
//------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysisFramework/Analyses/PointerAssignments.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DynamicRecursiveASTVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/ScalableStaticAnalysisFramework/Core/ASTEntityMapping.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Model/EntityName.h"
#include "clang/ScalableStaticAnalysisFramework/Core/Serialization/JSONFormat.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/ExtractorRegistry.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummary.h"
#include "clang/ScalableStaticAnalysisFramework/Core/TUSummary/TUSummaryBuilder.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Error.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"
#include <memory>
#include <type_traits>
#include <variant>

using namespace clang;
using namespace ssaf;

namespace clang::ssaf {
/////////////////////////////////////////////////////
// Declare Proxy functions
/////////////////////////////////////////////////////
class PointerAssignmentsTUSummaryExtractor;

extern Expected<std::unique_ptr<PointerAssignmentsEntitySummary>>
extractEntitySummary(PointerAssignmentsTUSummaryExtractor &Extractor,
                     const NamedDecl *Contributor, ASTContext &Ctx);

extern PointerAssignmentsTUSummaryExtractor *
createPointerAssignmentsTUSummaryExtractor(TUSummaryBuilder &Builder);

extern void deletePointerAssignmentsTUSummaryExtractor(
    PointerAssignmentsTUSummaryExtractor *);

extern EntityId addEntity(PointerAssignmentsTUSummaryExtractor &Extractor,
                          EntityName &EN);

class PointerAssignmentsTUSummaryExtractorProxy {
  PointerAssignmentsTUSummaryExtractor *Ptr;

public:
  explicit PointerAssignmentsTUSummaryExtractorProxy(TUSummaryBuilder &Builder)
      : Ptr(createPointerAssignmentsTUSummaryExtractor(Builder)) {}
  ~PointerAssignmentsTUSummaryExtractorProxy() {
    deletePointerAssignmentsTUSummaryExtractor(Ptr);
  }

  PointerAssignmentsTUSummaryExtractor &operator*() const { return *Ptr; }
};
} // namespace clang::ssaf

namespace {
// Use FindEntityByName to identify entities in unit tests.
// Unit tests are simple enough to meet the following assumptions:
// - Named declarations should have unique names, they can be found by comparing
//   names with strings;
// - Lambdas should initialize a variable named "X", they can be found using
//   "LambdaOfVar("X")";
// - CXX Ctors should have unique combination of names and number of parameters,
//   they can be found using "CXXCtorOfNumParms(name, numParms)".
struct LambdaOfVar {
  StringRef VarName;
};

struct CXXCtorOfNumParms {
  StringRef CXXCtorName;
  unsigned NumParms;
};

using FindEntityByName =
    std::variant<StringRef, CXXCtorOfNumParms, LambdaOfVar>;

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

StringRef toStringRef(const FindEntityByName &N) {
  return std::visit(
      Overloaded{
          [](StringRef S) -> StringRef { return S; },
          [](const CXXCtorOfNumParms &L) -> StringRef { return L.CXXCtorName; },
          [](const LambdaOfVar &L) -> StringRef { return L.VarName; },
      },
      N);
}

const NamedDecl *matchNamedDeclByFindEntityByName(const FindEntityByName &N,
                                                  const NamedDecl *D) {
  if (std::holds_alternative<LambdaOfVar>(N))
    D->dump();
  return std::visit(
      Overloaded{
          [&D](StringRef S) -> const NamedDecl * {
            if (D->getNameAsString() == S)
              return D;
            return nullptr;
          },
          [&D](const CXXCtorOfNumParms &L) -> const NamedDecl * {
            if (auto *CD = dyn_cast<CXXConstructorDecl>(D)) {
              if (CD->getNameAsString() == L.CXXCtorName &&
                  CD->getNumParams() == L.NumParms)
                return D;
            }
            return nullptr;
          },
          [&D](const LambdaOfVar &L) -> const NamedDecl * {
            if (const auto *VD = dyn_cast<VarDecl>(D); VD && VD->getInit()) {
              VD->dump();
              if (isa<LambdaExpr>(VD->getInit()) &&
                  VD->getNameAsString() == L.VarName)
                return cast<LambdaExpr>(VD->getInit())->getCallOperator();
            }
            return nullptr;
          },
      },
      N);
}

template <typename SomeDecl = NamedDecl,
          typename = std::enable_if_t<std::is_base_of_v<NamedDecl, SomeDecl>>>
const SomeDecl *findEntityByName(FindEntityByName Name, ASTContext &Ctx) {
  class NamedDeclFinder : public DynamicRecursiveASTVisitor {
  public:
    FindEntityByName SearchingName;
    const SomeDecl *FoundDecl = nullptr;

    NamedDeclFinder(FindEntityByName SearchingName)
        : SearchingName(SearchingName) {}

    bool VisitDecl(Decl *D) override {
      if (auto *ND = dyn_cast<NamedDecl>(D)) {
        FoundDecl = llvm::dyn_cast_or_null<SomeDecl>(
            matchNamedDeclByFindEntityByName(SearchingName, ND));
        if (FoundDecl)
          return false;
      }
      return true;
    }
  };

  NamedDeclFinder Finder(Name);

  Finder.TraverseDecl(Ctx.getTranslationUnitDecl());
  return dyn_cast_or_null<SomeDecl>(Finder.FoundDecl);
}

const FunctionDecl *findFnByName(FindEntityByName Name, ASTContext &Ctx) {
  return findEntityByName<FunctionDecl>(Name, Ctx);
}

// Same as `std::pair<StringName, unsigned>` for a pair of entity declaration
// name and a pointer level with an extra optional flag for whether the entity
// represents a function return value. This structure is used to explicitly
// spell out components of an EPL such as "{"p", 1}" or "{"foo_fn", 2, true}".
struct EPLPair {
  EPLPair(FindEntityByName Name, unsigned Lv, bool isFunRet = false)
      : Name(Name), Lv(Lv), isFunRet(isFunRet) {}

  FindEntityByName Name;
  unsigned Lv;
  bool isFunRet;
};

class PointerAssignmentsTest : public testing::Test {
protected:
  TUSummary TUSum;
  TUSummaryBuilder Builder;
  PointerAssignmentsTUSummaryExtractorProxy ExtractorProxy;
  std::unique_ptr<ASTUnit> AST;

  PointerAssignmentsTest()
      : TUSum(BuildNamespace(BuildNamespaceKind::CompilationUnit, "Mock.cpp")),
        Builder(TUSum), ExtractorProxy(Builder) {}

  template <typename ContributorDecl = NamedDecl,
            typename =
                std::enable_if_t<std::is_base_of_v<NamedDecl, ContributorDecl>>>
  std::unique_ptr<PointerAssignmentsEntitySummary>
  setUpTest(StringRef Code, FindEntityByName ContributorName) {
    AST = tooling::buildASTFromCodeWithArgs(
        Code, {"-Wno-unused-value", "-Wno-int-to-pointer-cast"});

    const auto *ContributorDefn = findEntityByName<ContributorDecl>(
        ContributorName, AST->getASTContext());

    if (!ContributorDefn)
      return nullptr;

    std::optional<EntityName> EN = getEntityName(ContributorDefn);

    if (!EN)
      return nullptr;

    auto Sum = extractEntitySummary(*ExtractorProxy, ContributorDefn,
                                    AST->getASTContext());
    if (!Sum) {
      llvm::consumeError(std::move(Sum.takeError()));
      return nullptr;
    }
    assert(*Sum);
    return std::move(*Sum);
  }

public:
  std::optional<EntityId> getEntityId(FindEntityByName Name) {
    if (const auto *D = findEntityByName(Name, AST->getASTContext())) {
      if (auto EntityName = getEntityName(D))
        return addEntity(*ExtractorProxy, *EntityName);
    }
    return std::nullopt;
  }

  std::optional<EntityId> getEntityIdForReturn(FindEntityByName FunName) {
    if (const auto *D = findFnByName(FunName, AST->getASTContext())) {
      if (auto EntityName = getEntityNameForReturn(D))
        return addEntity(*ExtractorProxy, *EntityName);
    }
    return std::nullopt;
  }

  EdgeSet makeEdges(unsigned Line, ArrayRef<std::pair<EPLPair, EPLPair>> Edges);
};

// 'ToEPL(Test, Line)' is a lambda that converts a 'EPLPair' to a
// 'EntityPointerLevel':
static constexpr auto ToEPL =
    [](PointerAssignmentsTest *Test,
       unsigned Line) -> std::function<EntityPointerLevel(const EPLPair &)> {
  return [Test, Line](const EPLPair &Pair) -> EntityPointerLevel {
    std::optional<EntityId> Entity = Pair.isFunRet
                                         ? Test->getEntityIdForReturn(Pair.Name)
                                         : Test->getEntityId(Pair.Name);
    if (!Entity) {
      ADD_FAILURE_AT(__FILE__, Line)
          << "Entity not found: " << toStringRef(Pair.Name);
    }
    return buildEntityPointerLevel(*Entity, Pair.Lv);
  };
};

EdgeSet
PointerAssignmentsTest::makeEdges(unsigned Line,
                                  ArrayRef<std::pair<EPLPair, EPLPair>> Edges) {
  EdgeSet Result;
  for (auto Edge : Edges)
    Result[ToEPL(this, Line)(Edge.first)].insert(
        ToEPL(this, Line)(Edge.second));
  return Result;
}

TEST_F(PointerAssignmentsTest, IsExtractorRegisteredTest) {
  EXPECT_TRUE(
      isTUSummaryExtractorRegistered("PointerAssignmentsTUSummaryExtractor"));
}

TEST_F(PointerAssignmentsTest, IsJSONFormatRegistered) {
  std::set<llvm::StringRef> ActualNames;
  for (const auto &Entry :
       llvm::Registry<clang::ssaf::JSONFormat::FormatInfo>::entries()) {
    bool Inserted = ActualNames.insert(Entry.getName()).second;
    EXPECT_TRUE(Inserted);
  }

  EXPECT_TRUE(ActualNames.count("PointerAssignments") == 1);
}

//////////////////////////////////////////////////////////////
//                     JSON Tests                           //
//////////////////////////////////////////////////////////////
// Oracle JSON output for the example:
// void foo(int ***p, int ****q, int x, int ****r) {
//   p[5][5][5];
//   q[5][5][5][5];
//   q[x] = p;
//   r = q;
// }
constexpr const char *const SerilizationTestOracle = R"cpp({
  "PointerAssignments": [
    [
      [
        {
          "@": 108
        },
        2
      ],
      [
        {
          "@": 42
        },
        1
      ]
    ],
    [
      [
        {
          "@": 9
        },
        1
      ],
      [
        {
          "@": 108
        },
        1
      ]
    ]
  ]
})cpp";

TEST_F(PointerAssignmentsTest, SerializeTest) {
  auto Sum = setUpTest(R"cpp(
    void foo(int ***p, int ****q, int x, int ****r) {
      p[5][5][5];
      q[5][5][5][5];
      q[x] = p;
      r = q;
    }
  )cpp",
                       "foo");
  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          /*[0]=*/{{"q", 2U}, {"p", 1U}},
                                          /*[1]=*/{{"r", 1U}, {"q", 1U}},
                                      }));

  using Object = llvm::json::Object;
  using Value = llvm::json::Value;
  std::map<EntityId, uint64_t> DummyTable{{*getEntityId("p"), 42},
                                          {*getEntityId("q"), 108},
                                          {*getEntityId("r"), 9}};

  Object JData = PointerAssignmentsEntitySummary::summaryToJSON(
      *Sum, [&DummyTable](EntityId Id) {
        return Object{{"@", Value(DummyTable[Id])}};
      });

  EXPECT_EQ(llvm::formatv("{0:2}", llvm::json::Value(std::move(JData))).str(),
            SerilizationTestOracle);
}

TEST_F(PointerAssignmentsTest, DeserializeTest) {
  auto Sum = setUpTest(R"cpp(
    void foo(int ***p, int ****q, int x, int ****r) {
      p[5][5][5];
      q[5][5][5][5];
      q[x] = p;
      r = q;
    }
  )cpp",
                       "foo");
  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          /*[0]=*/{{"q", 2U}, {"p", 1U}},
                                          /*[1]=*/{{"r", 1U}, {"q", 1U}},
                                      }));

  using Object = llvm::json::Object;
  using Value = llvm::json::Value;
  std::map<uint64_t, EntityId> DummyTable{{42, *getEntityId("p")},
                                          {108, *getEntityId("q")},
                                          {9, *getEntityId("r")}};
  Expected<Value> ParsedJSON = llvm::json::parse(SerilizationTestOracle);

  ASSERT_THAT_EXPECTED(ParsedJSON, llvm::Succeeded());
  ASSERT_NE(ParsedJSON->getAsObject(), nullptr);

  EntityIdTable Ignored;
  auto ParsedSum = PointerAssignmentsEntitySummary::summaryFromJSON(
      *ParsedJSON->getAsObject(), Ignored,
      [&DummyTable](const Object &O) -> Expected<EntityId> {
        return DummyTable.at(O.getInteger("@").value());
      });

  ASSERT_THAT_EXPECTED(ParsedSum, llvm::Succeeded());
  EXPECT_EQ(*static_cast<PointerAssignmentsEntitySummary *>(ParsedSum->get()),
            *Sum);
}

//////////////////////////////////////////////////////////////
//          Simple Assign Tests                             //
//////////////////////////////////////////////////////////////
TEST_F(PointerAssignmentsTest, SimpleAssign) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q) {
      q = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, AssignWithSubscriptLHS) {
  auto Sum = setUpTest(R"cpp(
    void foo(int **q, int *p, int x) {
      q[x] = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 2U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, AssignWithPtrArithRHS) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q) {
      q = p + 5;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, AssignInSubscript) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q) {
      (q = p)[5];
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, MultipleAssign) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q, int *r) {
      q = p;
      r = q;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"q", 1U}, {"p", 1U}},
                                          {{"r", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, ChainedAssign) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q, int *r) {
      r = q = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"q", 1U}, {"p", 1U}},
                                          {{"r", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, CastToRValue) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q) {
      q = static_cast<int *&&>(p);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, AssignToMember) {
  auto Sum = setUpTest(R"cpp(
    struct S { int *field; };
    void foo(S s, int *p) {
      s.field = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"field", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, AssignToMember2) {
  auto Sum = setUpTest(R"cpp(
    struct S { int *field; };
    void foo(S *s, int *p) {
      s->field = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"field", 1U}, {"p", 1U}}}));
}

//////////////////////////////////////////////////////////////
//          Call Expr Tests.                                //
//////////////////////////////////////////////////////////////
TEST_F(PointerAssignmentsTest, CallArg) {
  auto Sum = setUpTest(R"cpp(
    void bar(int *param);
    void foo(int *p) {
      bar(p);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"param", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, CallMultiArgs) {
  auto Sum = setUpTest(R"cpp(
    void bar(int *param1, int y, int *param2);
    void foo(int *p, int x, int *q) {
      bar(p, x, q);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"param1", 1U}, {"p", 1U}},
                                          {{"param2", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, CallAsCallArg) {
  auto Sum = setUpTest(R"cpp(

    int *bar(int * w);
    void foo(int * p) {
      foo(bar(p));
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"w", 1U}, {"p", 1U}},
                                       {{"p", 1U}, {"bar", 1U, true}}}));
}

TEST_F(PointerAssignmentsTest, CXXOperatorCallMultiArgs) {
  auto Sum = setUpTest(R"cpp(
    struct S {
      int* operator()(int *a, int *b);
    };
    void foo(S obj, int *p, int *q) {
      foo(obj, obj(p, q), obj(p, q));
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                          {{"p", 1U}, {"operator()", 1U, true}},
                                          {{"q", 1U}, {"operator()", 1U, true}},
                                      }));
}

TEST_F(PointerAssignmentsTest, CXXMemberCall) {
  auto Sum = setUpTest(R"cpp(
    struct S {
      int* method(int *a, int *b);
    };
    void foo(S obj, int *p, int *q) {
      foo(obj, obj.method(p, q), obj.method(p, q));
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"a", 1U}, {"p", 1U}},
                                       {{"b", 1U}, {"q", 1U}},
                                       {{"p", 1U}, {"method", 1U, true}},
                                       {{"q", 1U}, {"method", 1U, true}}}));
}

TEST_F(PointerAssignmentsTest, VirtualMethodCall) {
  auto Sum = setUpTest(R"cpp(
    struct Base {
      virtual void method(int *a);
    };
    void foo(Base &obj, int *p) {
      obj.method(p);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"a", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, StaticMethodCall) {
  auto Sum = setUpTest(R"cpp(
    struct S {
      static void method(int *a, int *b);
    };
    void foo(int *p, int *q) {
      S::method(p, q);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, DefaultArg) {
  auto Sum = setUpTest(R"cpp(
    int *g;
    void bar(int *a, int *b = g);
    void foo(int *p) {
      bar(p);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__,
                            {{{"a", 1U}, {"p", 1U}}, {{"b", 1U}, {"g", 1U}}}));
}

TEST_F(PointerAssignmentsTest, DefaultArg2) {
  auto Sum = setUpTest(R"cpp(
    int *g;
    void bar(int *a, int *b = g);
    void foo(int *p) {
      bar(p, p);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"p", 1U}},
                                      }));
}

//////////////////////////////////////////////////////////////
//          CXX Ctor Tests.                                 //
//////////////////////////////////////////////////////////////
TEST_F(PointerAssignmentsTest, CXXCtorCallMultiArgs) {
  auto Sum = setUpTest(R"cpp(
    struct S {
      S(int *a, int *b) {}
    };
    void foo(int *p, int *q) {
      S s{p, q};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, CXXCtorCallMultiArgs2) {
  auto Sum = setUpTest(R"cpp(
    struct S {
      S(int *a, int x, int *b) {}
    };
    void foo(int *p, int x, int *q) {
      S s{p, x, q};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, CXXCtorCallAsCallArg) {
  auto Sum = setUpTest(R"cpp(
    struct Wrapper {
      Wrapper(int *q) {}
    };
    void bar(Wrapper w);
    void foo(int *p) {
      bar(Wrapper{p});
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, DelegatingCXXCtorCall) {
  auto Sum = setUpTest<CXXConstructorDecl>(R"cpp(
    struct S {
      S(int *a, int *b) {}
      S(int *p) : S(p, p) {}
    };
  )cpp",
                                           CXXCtorOfNumParms{"S", 1});

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"p", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, CXXCtorBaseInit) {
  auto Sum = setUpTest<CXXConstructorDecl>(R"cpp(
    struct Base {
      Base(int *a) {}
    };
    struct Derived : Base {
      Derived(int *p) : Base(p) {}
    };
  )cpp",
                                           "Derived");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"a", 1U}, {"p", 1U}}}));
}

//////////////////////////////////////////////////////////////
//          Initializers Tests.                             //
//////////////////////////////////////////////////////////////
TEST_F(PointerAssignmentsTest, LocalVarDeclInit) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p) {
      int *q = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, LocalVarDeclInit2) {
  auto Sum = setUpTest(R"cpp(
    void foo(int (*arr)[10]) {
      int (*p)[10] = arr;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"p", 1U}, {"arr", 1U}}}));
}

TEST_F(PointerAssignmentsTest, FieldInit) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p) {
      struct Bar {
        int *field = p;
      };
    }
  )cpp",
                       "Bar");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"field", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, CXXCtorMemberInit) {
  StringRef Code = R"cpp(
    void foo(int *p) {
      struct Bar {
        int *member;
        Bar(int *q) : member(q) {}
      };
      Bar B{p};
    }
  )cpp";

  auto Sum = setUpTest<CXXConstructorDecl>(Code, "Bar");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"member", 1U}, {"q", 1U}}}));

  Sum = setUpTest(Code, "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"q", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, GlobalVarInit) {
  auto Sum = setUpTest<VarDecl>(R"cpp(
    int *q;
    int *g = q;
  )cpp",
                                "g");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"g", 1U}, {"q", 1U}}}));
}

TEST_F(PointerAssignmentsTest, StaticLocalInit) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p) {
      static int *s = p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"s", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, StaticMemberInit) {
  auto Sum = setUpTest<VarDecl>(R"cpp(
    int *g;
    struct S { static int *member = g; };   
  )cpp",
                                "member");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"member", 1U}, {"g", 1U}}}));
}

//////////////////////////////////////////////////////////////
//              InitList Tests.                               //
//////////////////////////////////////////////////////////////

TEST_F(PointerAssignmentsTest, ArrayInitList) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q) {
      int *arr[] = {p, q};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"arr", 2U}, {"p", 1U}},
                                          {{"arr", 2U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, StructInitList) {
  auto Sum = setUpTest(R"cpp(
    struct S { int *a; int *b; };
    void foo(int *p, int *q) {
      S s = {p, q};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                      }));
}

// A union initialized with a brace-enclosed initializer:
TEST_F(PointerAssignmentsTest, UnionInitList) {
  auto Sum = setUpTest(R"cpp(
    union U { int *x; int y; };
    void foo(int *p) {
      U u = {p};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"x", 1U}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, NestedInitList) {
  auto Sum = setUpTest(R"cpp(
    struct Inner { int * a; int * b; };
    struct S { Inner c; int * d; };
    void foo(int *p, int *q, int *r) {
      S s = {{p, q}, r};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                          {{"d", 1U}, {"r", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, NestedInitList2) {
  auto Sum = setUpTest(R"cpp(
    union Inner { int * a; int b; };
    struct S { Inner c; int * d; };
    void foo(int *p, int *q) {
      S s = {{p}, q};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"d", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, NestedInitList3) {
  auto Sum = setUpTest(R"cpp(
    struct Inner { int * a; int * b; };
    union S { Inner c; int * d; };
    void foo(int *p, int *q) {
      S s = {{p, q}};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, NestedArrayInitList) {
  auto Sum = setUpTest(R"cpp(
    void foo(int *p, int *q, int *r, int *s) {
      int *arr[][2] = {{p, q}, {r, s}};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"arr", 3U}, {"p", 1U}},
                                          {{"arr", 3U}, {"q", 1U}},
                                          {{"arr", 3U}, {"r", 1U}},
                                          {{"arr", 3U}, {"s", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, MixedNestedArrayStructInitList) {
  auto Sum = setUpTest(R"cpp(
    struct T { int *arr[2]; };
    void foo(int *p, int *q, int *r, int *s) {
      T t[2] = {{p, q}, {r, s}};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"arr", 2U}, {"p", 1U}},
                                          {{"arr", 2U}, {"q", 1U}},
                                          {{"arr", 2U}, {"r", 1U}},
                                          {{"arr", 2U}, {"s", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, ArrayOfStructInitList) {
  auto Sum = setUpTest(R"cpp(
    struct S { int *a; int *b; };
    void foo(int *p, int *q, int *r, int *s) {
      S arr[] = {{p, q}, {r, s}};
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"a", 1U}, {"p", 1U}},
                                          {{"b", 1U}, {"q", 1U}},
                                          {{"a", 1U}, {"r", 1U}},
                                          {{"b", 1U}, {"s", 1U}},
                                      }));
}

//////////////////////////////////////////////////////////////
//              Return Tests.                               //
//////////////////////////////////////////////////////////////

TEST_F(PointerAssignmentsTest, ReturnEdge) {
  auto Sum = setUpTest(R"cpp(
    int *foo(int *p) {
      return p;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"foo", 1U, true}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, MultipleReturnEdges) {
  auto Sum = setUpTest(R"cpp(
    int *foo(int *p, int *q, bool cond) {
      if (cond)
        return p;
      return q;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {
                                          {{"foo", 1U, true}, {"p", 1U}},
                                          {{"foo", 1U, true}, {"q", 1U}},
                                      }));
}

TEST_F(PointerAssignmentsTest, NoReturnEdgeForNonPointerReturnType) {
  auto Sum = setUpTest(R"cpp(
    int foo(int *p, int x) {
      return x;
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {}));
}

TEST_F(PointerAssignmentsTest, ReturnEdgeNotFromNestedFunction) {
  auto *Code = R"cpp(
    int *foo(int *p) {
      struct Inner {
        int *bar(int *q) { return q; }
      };
      return p;
    }
  )cpp";
  auto Sum = setUpTest(Code, "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"foo", 1U, true}, {"p", 1U}}}));

  Sum = setUpTest(Code, "bar");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"bar", 1U, true}, {"q", 1U}}}));
}

TEST_F(PointerAssignmentsTest, ReturnEdgeInClassMethod) {
  auto Sum = setUpTest(R"cpp(
  void foo() {
    struct S {
      int *method(int *p, int *q) { return p; }
    };
  }
  )cpp",
                       "method");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"method", 1U, true}, {"p", 1U}}}));
}

TEST_F(PointerAssignmentsTest, NoEdgeFromIndirectCall) {
  auto Sum = setUpTest(R"cpp(
    void bar(int *param1);
    void baz(int *param2);
    
    void foo(int *p, void (*fp)(int *)) {
      fp(p);
    }

    void main() {
      int *q;
      foo(q, bar);
      foo(q, baz);
    }
  )cpp",
                       "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(
      *Sum,
      makeEdges(
          __LINE__,
          /* FIXME or TBD: Currently indirect calls produce no edge: */ {}));
}

//////////////////////////////////////////////////////////////
//          Lambda Tests.                                   //
//////////////////////////////////////////////////////////////

TEST_F(PointerAssignmentsTest, ReturnInLambda) {
  StringRef Code = R"cpp(
    int* foo(int *p) {
      auto local = [](int *r) { return r; };
      return local(p);
    }
  )cpp";

  auto Sum = setUpTest(Code, "foo");

  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"r", 1U}, {"p", 1U}},
                                       {{"foo", 1U, true},
                                        {LambdaOfVar{"local"}, 1U, true}}}));

  Sum = setUpTest(Code, LambdaOfVar{"local"});
  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__,
                            {{{LambdaOfVar{"local"}, 1U, true}, {"r", 1U}}}));
}

TEST_F(PointerAssignmentsTest, NestedLambdaAssign) {
  StringRef Code = R"cpp(
    void foo() {
      auto outer_lambda = [](int *r, int *s) {
        s = r;
        auto inner_lambda = [](int *x, int *y) { y = x; };
      };
    }
  )cpp";

  auto Sum = setUpTest(Code, LambdaOfVar{"outer_lambda"});
  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"s", 1U}, {"r", 1U}}}));

  Sum = setUpTest(Code, LambdaOfVar{"inner_lambda"});
  ASSERT_NE(Sum, nullptr);
  EXPECT_EQ(*Sum, makeEdges(__LINE__, {{{"y", 1U}, {"x", 1U}}}));
}
} // namespace
