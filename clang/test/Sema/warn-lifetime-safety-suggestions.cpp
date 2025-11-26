// RUN: %clang_cc1 -fsyntax-only -fexperimental-lifetime-safety -Wexperimental-lifetime-safety-suggestions -verify %s

struct View;

struct [[gsl::Owner]] MyObj {
  int id;
  ~MyObj() {}  // Non-trivial destructor
  MyObj operator+(MyObj);

  View getView() const [[clang::lifetimebound]];
};

struct [[gsl::Pointer()]] View {
  View(const MyObj&); // Borrows from MyObj
  View();
  void use() const;
};

View return_view_directly (View a) {    // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
  return a;                             // expected-note {{param returned here}}
}

View conditional_return_view(
    View a,         // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
    View b,         // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
    bool c) {
  View res;
  if (c)  
    res = a;                    
  else
    res = b;          
  return res;  // expected-note 2 {{param returned here}} 
}

MyObj& return_reference(MyObj& a, // expected-warning {{param should be marked [[clang::lifetimebound]]}}
                        MyObj& b, // expected-warning {{param should be marked [[clang::lifetimebound]]}}
                        bool c) {
  if(c) {
    return a; // expected-note {{param returned here}}
  }
  return b;   // expected-note {{param returned here}}   
}

const MyObj& return_reference_const(const MyObj& a) { // expected-warning {{param should be marked [[clang::lifetimebound]]}}
  return a; // expected-note {{param returned here}}
}

MyObj* return_ptr_to_ref(MyObj& a) { // expected-warning {{param should be marked [[clang::lifetimebound]]}}
  return &a; // expected-note {{param returned here}}
}

// FIXME: Dereference does not propagate loans.
MyObj& return_ref_to_ptr(MyObj* a) {
  return *a;
}

View return_view_from_reference(MyObj& p) {  // expected-warning {{param should be marked [[clang::lifetimebound]]}}
  return p;  // expected-note {{param returned here}}
}

struct Container {  
  MyObj data;
  const MyObj& getData() [[clang::lifetimebound]] { return data; }
};
// FIXME: c.data does not forward loans
View return_struct_field(const Container& c) {
  return c.data;
}
View return_struct_lifetimebound_getter(Container& c) {  // expected-warning {{param should be marked [[clang::lifetimebound]]}}
  return c.getData().getView();  // expected-note {{param returned here}}
}

View return_view_from_reference_lifetimebound_member(MyObj& p) {  // expected-warning {{param should be marked [[clang::lifetimebound]]}}
  return p.getView();  // expected-note {{param returned here}}
}

int* return_pointer_directly (int* a) {    // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
  return a;                                // expected-note {{param returned here}} 
}

MyObj* return_pointer_object (MyObj* a) {  // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
  return a;                                // expected-note {{param returned here}} 
}

View only_one_paramter_annotated (View a [[clang::lifetimebound]], 
  View b,         // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
  bool c) {
 if(c)
  return a;
 return b;        // expected-note {{param returned here}} 
}

View reassigned_to_another_parameter (
    View a,
    View b) {     // expected-warning {{param should be marked [[clang::lifetimebound]]}}.
  a = b;
  return a;       // expected-note {{param returned here}} 
}

struct ReturnsSelf {
  const ReturnsSelf& get() const {
    return *this;
  }
};

struct ViewProvider {
  MyObj data;
  View getView() const {
    return data;
  }
};

// FIXME: Fails to generate lifetime suggestions for the implicit 'this' parameter, as this feature is not yet implemented.
void test_get_on_temporary() {
  const ReturnsSelf& s_ref = ReturnsSelf().get();
  (void)s_ref;
}

// FIXME: Fails to generate lifetime suggestions for the implicit 'this' parameter, as this feature is not yet implemented.
void test_getView_on_temporary() {
  View sv = ViewProvider{1}.getView();
  (void)sv;
}

//===----------------------------------------------------------------------===//
// Negative Test Cases
//===----------------------------------------------------------------------===//

View already_annotated(View a [[clang::lifetimebound]]) {
 return a;
}

MyObj return_obj_by_value(MyObj& p) {
  return p;
}

MyObj GlobalMyObj;
View Global = GlobalMyObj;
View Reassigned(View a) {
  a = Global;
  return a;
}
