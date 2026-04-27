===============================
Developer Guideline for AMDGPU
===============================

.. contents::
   :local:

Introduction
============

This document collects coding conventions, test policies, and other development
guidelines that apply to all AMDGPU-related code across the LLVM project (the
backend in ``llvm/lib/Target/AMDGPU``, Clang AMDGPU support, LLD, associated
tests, etc.).  It supplements the
`LLVM Coding Standards <https://llvm.org/docs/CodingStandards.html>`_ and the
`LLVM Testing Guide <https://llvm.org/docs/TestingGuide.html>`_; everything in
those documents still applies unless explicitly overridden here.

Some of the guidelines below overlap with rules in the upstream LLVM
documentation.  They are included here for convenience so that all expectations
for AMDGPU contributions are collected in one place.

Coding Standards
================

AMDGPU-related code follows the
`LLVM Coding Standards <https://llvm.org/docs/CodingStandards.html>`_ with the
refinements listed below.

Use of ``auto``
---------------

Do **not** use ``auto`` except in the following cases:

* Lambda expressions:

  .. code-block:: c++

     auto Pred = [](unsigned Val) { return Val > 0; };

* Casts where the target type is already spelled out on the right-hand side
  (``cast``, ``dyn_cast``, ``static_cast``, etc.):

  .. code-block:: c++

     auto *Inst = cast<CallInst>(V);

* Iterators:

  .. code-block:: c++

     auto It = Container.begin();

* Structured bindings:

  .. code-block:: c++

     auto [Key, Value] = Pair;

In all other cases, write the type explicitly.

Use of Braces
-------------

LLVM's coding standard allows omitting braces around a single-statement block.
In AMDGPU code, braces may be omitted **only when the single statement
fits on one line**.  If the statement spans more than one line (e.g. because of
a long argument list that wraps), keep the braces.

For ``if``/``else`` chains, if **either** branch requires braces, add braces to
**all** branches to keep them symmetric.

.. code-block:: c++

   // OK — single statement on one line, braces omitted.
   for (unsigned I = 0; I < N; ++I)
     doSomething(I);

   // Required — single statement, but spans multiple lines.
   if (Cond) {
     doSomethingElse(LongArgument1,
                     LongArgument2);
   }

   // Required — the else branch needs braces, so the if branch gets them too.
   if (Cond) {
     doSomething();
   } else {
     doSomethingElse(LongArgument1,
                     LongArgument2);
   }

   // Avoid — asymmetric braces.
   if (Cond)
     doSomething();
   else {
     doSomethingElse(LongArgument1,
                     LongArgument2);
   }

Design Practices
=================

Prefer Feature Checks over Generation Checks
---------------------------------------------

Avoid conditioning logic on a specific GPU generation (e.g. ``isGFX11()``).
Generation checks are fragile: when a new generation ships, every such check
must be audited to decide whether it should include the new generation too.

Instead, query the specific capability that the code actually depends on via a
feature predicate (e.g. ``hasFeatureX()``).  Feature predicates are
self-documenting, compose better across generations, and do not require updates
when a new generation is added.

.. code-block:: c++

   // Avoid — ties the logic to a specific generation.
   if (ST.isGFX11())
     handleNewBehaviour();

   // Preferred — checks the actual capability.
   if (ST.hasPackedFP32Ops())
     handleNewBehaviour();

Pull Requests
=============

Keep Changes Focused
--------------------

Each pull request should contain **one logical change**.  Unrelated
modifications — formatting fixes, variable renames, whitespace cleanup, etc. —
should be submitted as separate PRs, even if they touch the same files.  Mixing
unrelated changes into a functional PR makes review harder, obscures the intent
of the change in ``git log``, and complicates reverts if something goes wrong.

Test Policy
===========

Well-written tests are essential for a healthy codebase.  The guidelines below
apply to all AMDGPU regression tests (``llvm/test/CodeGen/AMDGPU``,
``llvm/test/MC/AMDGPU``, etc.).  See also the general
`Best practices for regression tests <https://llvm.org/docs/TestingGuide.html#best-practices-for-regression-tests>`_
and the
`Precommit workflow for tests <https://llvm.org/docs/TestingGuide.html#precommit-workflow-for-tests>`_
in the LLVM Testing Guide.

Use Minimal, Reduced Tests
--------------------------

Every test should be the **smallest input that exercises the behaviour under
test**.  Avoid copying a full function from a real workload and pasting it into
a test file.  Instead, reduce the input so that it contains only the
instructions and control flow needed to trigger the relevant code path.  A
minimal test is easier to understand, faster to run, and less likely to break
for unrelated reasons.

Avoid Undefined Behavior
------------------------

Tests should not rely on undefined behavior (UB).  As the
`best practices section of the Testing Guide <https://llvm.org/docs/TestingGuide.html#best-practices-for-regression-tests>`_
notes, avoid ``undef`` and ``poison`` values unless they are the point of the
test — patterns like ``br i1 undef`` are likely to break as future
optimizations evolve.

In addition, avoid loads from or stores to ``null`` unless the test targets an
address space where address zero is a valid memory location rather than a null
pointer.  For example, on AMDGPU, address space 0 (generic/flat) treats zero as
a null pointer, but address space 3 (LDS) does not, so a load from
``ptr addrspace(3) null`` can be valid.

.. code-block:: llvm

   ; Avoid — null is a null pointer in addrspace(0).
   define void @example_bad() {
     %val = load i32, ptr null
     ret void
   }

   ; OK — addrspace(3) has no null pointer, address zero is valid.
   define void @example_ok() {
     %val = load i32, ptr addrspace(3) null
     ret void
   }

Use Named Values
----------------

Prefer descriptive, named IR values over anonymous numbered values.  Names
serve as lightweight documentation and make it much easier to understand a
test's intent at a glance.

.. code-block:: llvm

   ; Preferred — names describe what each value represents.
   define float @fma_example(float %x, float %y, float %z) {
     %fma = call float @llvm.fma.f32(float %x, float %y, float %z)
     ret float %fma
   }

   ; Avoid — anonymous numbers reveal nothing about intent.
   define float @fma_example(float %0, float %1, float %2) {
     %4 = call float @llvm.fma.f32(float %0, float %1, float %2)
     ret float %4
   }

Trim Unnecessary Attributes and Metadata
-----------------------------------------

Strip attributes, metadata, and other annotations that are **not relevant to
the behaviour being tested**.  Extra noise makes it harder to see what a test
actually depends on and can cause spurious failures when defaults change.

For example, unless a test specifically exercises a particular function
attribute or metadata node, remove them:

.. code-block:: llvm

   ; Preferred — only the essential attributes remain.
   define amdgpu_kernel void @store_i32(ptr addrspace(1) %ptr, i32 %val) {
     store i32 %val, ptr addrspace(1) %ptr
     ret void
   }

   ; Avoid — unrelated attributes and metadata obscure the test's purpose.
   define amdgpu_kernel void @store_i32(ptr addrspace(1) %ptr, i32 %val) #0 !dbg !5 {
     store i32 %val, ptr addrspace(1) %ptr, align 4, !tbaa !11
     ret void
   }

   attributes #0 = { nounwind "frame-pointer"="all" }

Pipe Input via ``stdin``
------------------------

In ``llc`` and ``opt`` RUN lines, feed the test file through ``stdin`` using
``< %s`` rather than passing it as a positional argument with ``-o -``.  Reading
from ``stdin`` is the conventional style in AMDGPU tests and avoids the need for
an explicit ``-o -``.

.. code-block:: bash

   ; Avoid — file argument with -o -.
   ; RUN: llc -mtriple=amdgcn -mcpu=gfx900 -o - %s | FileCheck %s

   ; Preferred — pipe via stdin.
   ; RUN: llc -mtriple=amdgcn -mcpu=gfx900 < %s | FileCheck %s

Use ``-filetype=null`` When Output Is Irrelevant
-------------------------------------------------

When a test only needs to verify diagnostics, error messages, or the absence of
a crash — and does not care about the actual code-generation output — pass
``-filetype=null`` to ``llc``.  This skips object or assembly emission entirely,
making the test faster and avoiding fragile ``CHECK`` lines tied to unrelated
output.

.. code-block:: bash

   ; RUN: llc -mtriple=amdgcn -mcpu=gfx900 -filetype=null %s 2>&1 | FileCheck %s

Auto-Generate Check Lines
-------------------------

When possible, use the UTC (Update Test Checks) scripts to generate ``CHECK``
lines rather than writing them by hand.  Auto-generated checks are
comprehensive, consistent, and easy to update when output changes.

The most commonly used scripts for AMDGPU are:

* ``llvm/utils/update_llc_test_checks.py`` — for ``llc`` CodeGen tests.
* ``llvm/utils/update_mir_test_checks.py`` — for MIR tests.
* ``llvm/utils/update_mc_test_checks.py`` — for MC (assembly/disassembly) tests.

A typical workflow looks like:

.. code-block:: bash

   # Write the test with a RUN line but no CHECK lines, then auto-generate:
   $ llvm/utils/update_llc_test_checks.py llvm/test/CodeGen/AMDGPU/my-test.ll

   # After a code change that intentionally alters output, re-generate:
   $ llvm/utils/update_llc_test_checks.py --update-only llvm/test/CodeGen/AMDGPU/my-test.ll

Each script embeds a ``UTC_ARGS:`` comment in the test file so that subsequent
runs of the script use the same options.  Consult the ``--help`` output of each
script for the full set of available flags.

.. note::

   Hand-written ``CHECK`` lines are still appropriate when a test needs to
   verify only a narrow slice of the output (e.g. a single instruction) or
   when the auto-generated output would be excessively verbose and obscure the
   intent.  In such cases, keep the hand-written checks focused and document
   why auto-generation was not used.
