// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/resolver/resolver.h"

#include <tuple>

#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "src/ast/assignment_statement.h"
#include "src/ast/bitcast_expression.h"
#include "src/ast/break_statement.h"
#include "src/ast/call_statement.h"
#include "src/ast/continue_statement.h"
#include "src/ast/if_statement.h"
#include "src/ast/intrinsic_texture_helper_test.h"
#include "src/ast/loop_statement.h"
#include "src/ast/return_statement.h"
#include "src/ast/stage_decoration.h"
#include "src/ast/struct_block_decoration.h"
#include "src/ast/switch_statement.h"
#include "src/ast/unary_op_expression.h"
#include "src/ast/variable_decl_statement.h"
#include "src/resolver/resolver_test_helper.h"
#include "src/sem/access_control_type.h"
#include "src/sem/call.h"
#include "src/sem/function.h"
#include "src/sem/member_accessor_expression.h"
#include "src/sem/sampled_texture_type.h"
#include "src/sem/statement.h"
#include "src/sem/variable.h"

using ::testing::ElementsAre;
using ::testing::HasSubstr;

namespace tint {
namespace resolver {
namespace {

// Helpers and typedefs
using i32 = ProgramBuilder::i32;
using u32 = ProgramBuilder::u32;
using f32 = ProgramBuilder::f32;
using Op = ast::BinaryOp;

TEST_F(ResolverTest, Stmt_Assign) {
  auto* v = Var("v", ty.f32(), ast::StorageClass::kFunction);
  auto* lhs = Expr("v");
  auto* rhs = Expr(2.3f);

  auto* assign = create<ast::AssignmentStatement>(lhs, rhs);
  WrapInFunction(v, assign);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(lhs), nullptr);
  ASSERT_NE(TypeOf(rhs), nullptr);

  EXPECT_TRUE(TypeOf(lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(rhs)->Is<sem::F32>());
  EXPECT_EQ(StmtOf(lhs), assign);
  EXPECT_EQ(StmtOf(rhs), assign);
}

TEST_F(ResolverTest, Stmt_Case) {
  auto* v = Var("v", ty.f32(), ast::StorageClass::kFunction);
  auto* lhs = Expr("v");
  auto* rhs = Expr(2.3f);

  auto* assign = create<ast::AssignmentStatement>(lhs, rhs);
  auto* block = Block(assign);
  ast::CaseSelectorList lit;
  lit.push_back(create<ast::SintLiteral>(ty.i32(), 3));
  auto* cse = create<ast::CaseStatement>(lit, block);
  WrapInFunction(v, cse);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(lhs), nullptr);
  ASSERT_NE(TypeOf(rhs), nullptr);
  EXPECT_TRUE(TypeOf(lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(rhs)->Is<sem::F32>());
  EXPECT_EQ(StmtOf(lhs), assign);
  EXPECT_EQ(StmtOf(rhs), assign);
  EXPECT_EQ(BlockOf(assign), block);
}

TEST_F(ResolverTest, Stmt_Block) {
  auto* v = Var("v", ty.f32(), ast::StorageClass::kFunction);
  auto* lhs = Expr("v");
  auto* rhs = Expr(2.3f);

  auto* assign = create<ast::AssignmentStatement>(lhs, rhs);
  auto* block = Block(assign);
  WrapInFunction(v, block);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(lhs), nullptr);
  ASSERT_NE(TypeOf(rhs), nullptr);
  EXPECT_TRUE(TypeOf(lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(rhs)->Is<sem::F32>());
  EXPECT_EQ(StmtOf(lhs), assign);
  EXPECT_EQ(StmtOf(rhs), assign);
  EXPECT_EQ(BlockOf(lhs), block);
  EXPECT_EQ(BlockOf(rhs), block);
  EXPECT_EQ(BlockOf(assign), block);
}

TEST_F(ResolverTest, Stmt_If) {
  auto* v = Var("v", ty.f32(), ast::StorageClass::kFunction);
  auto* else_lhs = Expr("v");
  auto* else_rhs = Expr(2.3f);

  auto* else_body = Block(create<ast::AssignmentStatement>(else_lhs, else_rhs));

  auto* else_cond = Expr(3);
  auto* else_stmt = create<ast::ElseStatement>(else_cond, else_body);

  auto* lhs = Expr("v");
  auto* rhs = Expr(2.3f);

  auto* assign = create<ast::AssignmentStatement>(lhs, rhs);
  auto* body = Block(assign);
  auto* cond = Expr(true);
  auto* stmt =
      create<ast::IfStatement>(cond, body, ast::ElseStatementList{else_stmt});
  WrapInFunction(v, stmt);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(stmt->condition()), nullptr);
  ASSERT_NE(TypeOf(else_lhs), nullptr);
  ASSERT_NE(TypeOf(else_rhs), nullptr);
  ASSERT_NE(TypeOf(lhs), nullptr);
  ASSERT_NE(TypeOf(rhs), nullptr);
  EXPECT_TRUE(TypeOf(stmt->condition())->Is<sem::Bool>());
  EXPECT_TRUE(TypeOf(else_lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(else_rhs)->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(rhs)->Is<sem::F32>());
  EXPECT_EQ(StmtOf(lhs), assign);
  EXPECT_EQ(StmtOf(rhs), assign);
  EXPECT_EQ(StmtOf(cond), stmt);
  EXPECT_EQ(StmtOf(else_cond), else_stmt);
  EXPECT_EQ(BlockOf(lhs), body);
  EXPECT_EQ(BlockOf(rhs), body);
  EXPECT_EQ(BlockOf(else_lhs), else_body);
  EXPECT_EQ(BlockOf(else_rhs), else_body);
}

TEST_F(ResolverTest, Stmt_Loop) {
  auto* v = Var("v", ty.f32(), ast::StorageClass::kFunction);
  auto* body_lhs = Expr("v");
  auto* body_rhs = Expr(2.3f);

  auto* body = Block(create<ast::AssignmentStatement>(body_lhs, body_rhs));
  auto* continuing_lhs = Expr("v");
  auto* continuing_rhs = Expr(2.3f);

  auto* continuing = create<ast::BlockStatement>(ast::StatementList{
      create<ast::AssignmentStatement>(continuing_lhs, continuing_rhs),
  });
  auto* stmt = create<ast::LoopStatement>(body, continuing);
  WrapInFunction(v, stmt);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(body_lhs), nullptr);
  ASSERT_NE(TypeOf(body_rhs), nullptr);
  ASSERT_NE(TypeOf(continuing_lhs), nullptr);
  ASSERT_NE(TypeOf(continuing_rhs), nullptr);
  EXPECT_TRUE(TypeOf(body_lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(body_rhs)->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(continuing_lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(continuing_rhs)->Is<sem::F32>());
  EXPECT_EQ(BlockOf(body_lhs), body);
  EXPECT_EQ(BlockOf(body_rhs), body);
  EXPECT_EQ(BlockOf(continuing_lhs), continuing);
  EXPECT_EQ(BlockOf(continuing_rhs), continuing);
}

TEST_F(ResolverTest, Stmt_Return) {
  auto* cond = Expr(2);

  auto* ret = create<ast::ReturnStatement>(cond);
  Func("test", {}, ty.i32(), {ret}, {});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(cond), nullptr);
  EXPECT_TRUE(TypeOf(cond)->Is<sem::I32>());
}

TEST_F(ResolverTest, Stmt_Return_WithoutValue) {
  auto* ret = create<ast::ReturnStatement>();
  WrapInFunction(ret);

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverTest, Stmt_Switch) {
  auto* v = Var("v", ty.f32(), ast::StorageClass::kFunction);
  auto* lhs = Expr("v");
  auto* rhs = Expr(2.3f);
  auto* case_block = Block(Assign(lhs, rhs));
  auto* stmt = Switch(Expr(2), Case(Literal(3), case_block), DefaultCase());
  WrapInFunction(v, stmt);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(stmt->condition()), nullptr);
  ASSERT_NE(TypeOf(lhs), nullptr);
  ASSERT_NE(TypeOf(rhs), nullptr);

  EXPECT_TRUE(TypeOf(stmt->condition())->Is<sem::I32>());
  EXPECT_TRUE(TypeOf(lhs)->UnwrapAll()->Is<sem::F32>());
  EXPECT_TRUE(TypeOf(rhs)->Is<sem::F32>());
  EXPECT_EQ(BlockOf(lhs), case_block);
  EXPECT_EQ(BlockOf(rhs), case_block);
}

TEST_F(ResolverTest, Stmt_Call) {
  ast::VariableList params;
  Func("my_func", params, ty.f32(), ast::StatementList{Return(Expr(0.0f))},
       ast::DecorationList{});

  auto* expr = Call("my_func");

  auto* call = create<ast::CallStatement>(expr);
  WrapInFunction(call);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(expr), nullptr);
  EXPECT_TRUE(TypeOf(expr)->Is<sem::F32>());
  EXPECT_EQ(StmtOf(expr), call);
}

TEST_F(ResolverTest, Stmt_VariableDecl) {
  auto* var = Var("my_var", ty.i32(), ast::StorageClass::kNone, Expr(2));
  auto* init = var->constructor();

  auto* decl = create<ast::VariableDeclStatement>(var);
  WrapInFunction(decl);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(init), nullptr);
  EXPECT_TRUE(TypeOf(init)->Is<sem::I32>());
}

TEST_F(ResolverTest, Stmt_VariableDecl_Alias) {
  auto* my_int = ty.alias("MyInt", ty.i32());
  auto* var = Var("my_var", my_int, ast::StorageClass::kNone, Expr(2));
  auto* init = var->constructor();

  auto* decl = create<ast::VariableDeclStatement>(var);
  WrapInFunction(decl);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(init), nullptr);
  EXPECT_TRUE(TypeOf(init)->Is<sem::I32>());
}

TEST_F(ResolverTest, Stmt_VariableDecl_ModuleScope) {
  auto* init = Expr(2);
  Global("my_var", ty.i32(), ast::StorageClass::kInput, init);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(init), nullptr);
  EXPECT_TRUE(TypeOf(init)->Is<sem::I32>());
  EXPECT_EQ(StmtOf(init), nullptr);
}

TEST_F(ResolverTest, Stmt_VariableDecl_OuterScopeAfterInnerScope) {
  // fn func_i32() {
  //   {
  //     var foo : i32 = 2;
  //     var bar : i32 = foo;
  //   }
  //   var foo : f32 = 2.0;
  //   var bar : f32 = foo;
  // }

  ast::VariableList params;

  // Declare i32 "foo" inside a block
  auto* foo_i32 = Var("foo", ty.i32(), ast::StorageClass::kNone, Expr(2));
  auto* foo_i32_init = foo_i32->constructor();
  auto* foo_i32_decl = create<ast::VariableDeclStatement>(foo_i32);

  // Reference "foo" inside the block
  auto* bar_i32 = Var("bar", ty.i32(), ast::StorageClass::kNone, Expr("foo"));
  auto* bar_i32_init = bar_i32->constructor();
  auto* bar_i32_decl = create<ast::VariableDeclStatement>(bar_i32);

  auto* inner = create<ast::BlockStatement>(
      ast::StatementList{foo_i32_decl, bar_i32_decl});

  // Declare f32 "foo" at function scope
  auto* foo_f32 = Var("foo", ty.f32(), ast::StorageClass::kNone, Expr(2.f));
  auto* foo_f32_init = foo_f32->constructor();
  auto* foo_f32_decl = create<ast::VariableDeclStatement>(foo_f32);

  // Reference "foo" at function scope
  auto* bar_f32 = Var("bar", ty.f32(), ast::StorageClass::kNone, Expr("foo"));
  auto* bar_f32_init = bar_f32->constructor();
  auto* bar_f32_decl = create<ast::VariableDeclStatement>(bar_f32);

  Func("func", params, ty.void_(),
       ast::StatementList{inner, foo_f32_decl, bar_f32_decl},
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
  ASSERT_NE(TypeOf(foo_i32_init), nullptr);
  EXPECT_TRUE(TypeOf(foo_i32_init)->Is<sem::I32>());
  ASSERT_NE(TypeOf(foo_f32_init), nullptr);
  EXPECT_TRUE(TypeOf(foo_f32_init)->Is<sem::F32>());
  ASSERT_NE(TypeOf(bar_i32_init), nullptr);
  EXPECT_TRUE(TypeOf(bar_i32_init)->UnwrapAll()->Is<sem::I32>());
  ASSERT_NE(TypeOf(bar_f32_init), nullptr);
  EXPECT_TRUE(TypeOf(bar_f32_init)->UnwrapAll()->Is<sem::F32>());
  EXPECT_EQ(StmtOf(foo_i32_init), foo_i32_decl);
  EXPECT_EQ(StmtOf(bar_i32_init), bar_i32_decl);
  EXPECT_EQ(StmtOf(foo_f32_init), foo_f32_decl);
  EXPECT_EQ(StmtOf(bar_f32_init), bar_f32_decl);
  EXPECT_TRUE(CheckVarUsers(foo_i32, {bar_i32->constructor()}));
  EXPECT_TRUE(CheckVarUsers(foo_f32, {bar_f32->constructor()}));
  ASSERT_NE(VarOf(bar_i32->constructor()), nullptr);
  EXPECT_EQ(VarOf(bar_i32->constructor())->Declaration(), foo_i32);
  ASSERT_NE(VarOf(bar_f32->constructor()), nullptr);
  EXPECT_EQ(VarOf(bar_f32->constructor())->Declaration(), foo_f32);
}

TEST_F(ResolverTest, Stmt_VariableDecl_ModuleScopeAfterFunctionScope) {
  // fn func_i32() {
  //   var foo : i32 = 2;
  // }
  // var foo : f32 = 2.0;
  // fn func_f32() {
  //   var bar : f32 = foo;
  // }

  ast::VariableList params;

  // Declare i32 "foo" inside a function
  auto* fn_i32 = Var("foo", ty.i32(), ast::StorageClass::kFunction, Expr(2));
  auto* fn_i32_init = fn_i32->constructor();
  auto* fn_i32_decl = create<ast::VariableDeclStatement>(fn_i32);
  Func("func_i32", params, ty.void_(), ast::StatementList{fn_i32_decl},
       ast::DecorationList{});

  // Declare f32 "foo" at module scope
  auto* mod_f32 = Var("foo", ty.f32(), ast::StorageClass::kInput, Expr(2.f));
  auto* mod_init = mod_f32->constructor();
  AST().AddGlobalVariable(mod_f32);

  // Reference "foo" in another function
  auto* fn_f32 =
      Var("bar", ty.f32(), ast::StorageClass::kFunction, Expr("foo"));
  auto* fn_f32_init = fn_f32->constructor();
  auto* fn_f32_decl = create<ast::VariableDeclStatement>(fn_f32);
  Func("func_f32", params, ty.void_(), ast::StatementList{fn_f32_decl},
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
  ASSERT_NE(TypeOf(mod_init), nullptr);
  EXPECT_TRUE(TypeOf(mod_init)->Is<sem::F32>());
  ASSERT_NE(TypeOf(fn_i32_init), nullptr);
  EXPECT_TRUE(TypeOf(fn_i32_init)->Is<sem::I32>());
  ASSERT_NE(TypeOf(fn_f32_init), nullptr);
  EXPECT_TRUE(TypeOf(fn_f32_init)->UnwrapAll()->Is<sem::F32>());
  EXPECT_EQ(StmtOf(fn_i32_init), fn_i32_decl);
  EXPECT_EQ(StmtOf(mod_init), nullptr);
  EXPECT_EQ(StmtOf(fn_f32_init), fn_f32_decl);
  EXPECT_TRUE(CheckVarUsers(fn_i32, {}));
  EXPECT_TRUE(CheckVarUsers(mod_f32, {fn_f32->constructor()}));
  ASSERT_NE(VarOf(fn_f32->constructor()), nullptr);
  EXPECT_EQ(VarOf(fn_f32->constructor())->Declaration(), mod_f32);
}

TEST_F(ResolverTest, Expr_ArrayAccessor_Array) {
  auto* idx = Expr(2);
  Global("my_var", ty.array<f32, 3>(), ast::StorageClass::kFunction);

  auto* acc = IndexAccessor("my_var", idx);
  WrapInFunction(acc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(acc), nullptr);
  ASSERT_TRUE(TypeOf(acc)->Is<sem::Pointer>());

  auto* ptr = TypeOf(acc)->As<sem::Pointer>();
  EXPECT_TRUE(ptr->type()->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_ArrayAccessor_Alias_Array) {
  auto* aary = ty.alias("myarrty", ty.array<f32, 3>());

  Global("my_var", aary, ast::StorageClass::kFunction);

  auto* acc = IndexAccessor("my_var", 2);
  WrapInFunction(acc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(acc), nullptr);
  ASSERT_TRUE(TypeOf(acc)->Is<sem::Pointer>());

  auto* ptr = TypeOf(acc)->As<sem::Pointer>();
  EXPECT_TRUE(ptr->type()->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_ArrayAccessor_Array_Constant) {
  GlobalConst("my_var", ty.array<f32, 3>());

  auto* acc = IndexAccessor("my_var", 2);
  WrapInFunction(acc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(acc), nullptr);
  EXPECT_TRUE(TypeOf(acc)->Is<sem::F32>()) << TypeOf(acc)->type_name();
}

TEST_F(ResolverTest, Expr_ArrayAccessor_Matrix) {
  Global("my_var", ty.mat2x3<f32>(), ast::StorageClass::kInput);

  auto* acc = IndexAccessor("my_var", 2);
  WrapInFunction(acc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(acc), nullptr);
  ASSERT_TRUE(TypeOf(acc)->Is<sem::Pointer>());

  auto* ptr = TypeOf(acc)->As<sem::Pointer>();
  ASSERT_TRUE(ptr->type()->Is<sem::Vector>());
  EXPECT_EQ(ptr->type()->As<sem::Vector>()->size(), 3u);
}

TEST_F(ResolverTest, Expr_ArrayAccessor_Matrix_BothDimensions) {
  Global("my_var", ty.mat2x3<f32>(), ast::StorageClass::kInput);

  auto* acc = IndexAccessor(IndexAccessor("my_var", 2), 1);
  WrapInFunction(acc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(acc), nullptr);
  ASSERT_TRUE(TypeOf(acc)->Is<sem::Pointer>());

  auto* ptr = TypeOf(acc)->As<sem::Pointer>();
  EXPECT_TRUE(ptr->type()->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_ArrayAccessor_Vector) {
  Global("my_var", ty.vec3<f32>(), ast::StorageClass::kInput);

  auto* acc = IndexAccessor("my_var", 2);
  WrapInFunction(acc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(acc), nullptr);
  ASSERT_TRUE(TypeOf(acc)->Is<sem::Pointer>());

  auto* ptr = TypeOf(acc)->As<sem::Pointer>();
  EXPECT_TRUE(ptr->type()->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Bitcast) {
  Global("name", ty.f32(), ast::StorageClass::kPrivate);

  auto* bitcast = create<ast::BitcastExpression>(ty.f32(), Expr("name"));
  WrapInFunction(bitcast);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(bitcast), nullptr);
  EXPECT_TRUE(TypeOf(bitcast)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Call) {
  ast::VariableList params;
  Func("my_func", params, ty.f32(), ast::StatementList{Return(Expr(0.0f))},
       ast::DecorationList{});

  auto* call = Call("my_func");
  WrapInFunction(call);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(call), nullptr);
  EXPECT_TRUE(TypeOf(call)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Call_InBinaryOp) {
  ast::VariableList params;
  Func("func", params, ty.f32(), ast::StatementList{Return(Expr(0.0f))},
       ast::DecorationList{});

  auto* expr = Add(Call("func"), Call("func"));
  WrapInFunction(expr);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(expr), nullptr);
  EXPECT_TRUE(TypeOf(expr)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Call_WithParams) {
  ast::VariableList params;
  Func("my_func", params, ty.void_(), ast::StatementList{},
       ast::DecorationList{});

  auto* param = Expr(2.4f);

  auto* call = Call("my_func", param);
  WrapInFunction(call);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(param), nullptr);
  EXPECT_TRUE(TypeOf(param)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Call_Intrinsic) {
  auto* call = Call("round", 2.4f);
  WrapInFunction(call);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(call), nullptr);
  EXPECT_TRUE(TypeOf(call)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Cast) {
  Global("name", ty.f32(), ast::StorageClass::kPrivate);

  auto* cast = Construct(ty.f32(), "name");
  WrapInFunction(cast);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(cast), nullptr);
  EXPECT_TRUE(TypeOf(cast)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Constructor_Scalar) {
  auto* s = Expr(1.0f);
  WrapInFunction(s);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(s), nullptr);
  EXPECT_TRUE(TypeOf(s)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Constructor_Type_Vec2) {
  auto* tc = vec2<f32>(1.0f, 1.0f);
  WrapInFunction(tc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(tc), nullptr);
  ASSERT_TRUE(TypeOf(tc)->Is<sem::Vector>());
  EXPECT_TRUE(TypeOf(tc)->As<sem::Vector>()->type()->Is<sem::F32>());
  EXPECT_EQ(TypeOf(tc)->As<sem::Vector>()->size(), 2u);
}

TEST_F(ResolverTest, Expr_Constructor_Type_Vec3) {
  auto* tc = vec3<f32>(1.0f, 1.0f, 1.0f);
  WrapInFunction(tc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(tc), nullptr);
  ASSERT_TRUE(TypeOf(tc)->Is<sem::Vector>());
  EXPECT_TRUE(TypeOf(tc)->As<sem::Vector>()->type()->Is<sem::F32>());
  EXPECT_EQ(TypeOf(tc)->As<sem::Vector>()->size(), 3u);
}

TEST_F(ResolverTest, Expr_Constructor_Type_Vec4) {
  auto* tc = vec4<f32>(1.0f, 1.0f, 1.0f, 1.0f);
  WrapInFunction(tc);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(tc), nullptr);
  ASSERT_TRUE(TypeOf(tc)->Is<sem::Vector>());
  EXPECT_TRUE(TypeOf(tc)->As<sem::Vector>()->type()->Is<sem::F32>());
  EXPECT_EQ(TypeOf(tc)->As<sem::Vector>()->size(), 4u);
}

TEST_F(ResolverTest, Expr_Identifier_GlobalVariable) {
  auto* my_var = Global("my_var", ty.f32(), ast::StorageClass::kInput);

  auto* ident = Expr("my_var");
  WrapInFunction(ident);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(ident), nullptr);
  EXPECT_TRUE(TypeOf(ident)->Is<sem::Pointer>());
  EXPECT_TRUE(TypeOf(ident)->As<sem::Pointer>()->type()->Is<sem::F32>());
  EXPECT_TRUE(CheckVarUsers(my_var, {ident}));
  ASSERT_NE(VarOf(ident), nullptr);
  EXPECT_EQ(VarOf(ident)->Declaration(), my_var);
}

TEST_F(ResolverTest, Expr_Identifier_GlobalConstant) {
  auto* my_var = GlobalConst("my_var", ty.f32());

  auto* ident = Expr("my_var");
  WrapInFunction(ident);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(ident), nullptr);
  EXPECT_TRUE(TypeOf(ident)->Is<sem::F32>());
  EXPECT_TRUE(CheckVarUsers(my_var, {ident}));
  ASSERT_NE(VarOf(ident), nullptr);
  EXPECT_EQ(VarOf(ident)->Declaration(), my_var);
}

TEST_F(ResolverTest, Expr_Identifier_FunctionVariable_Const) {
  auto* my_var_a = Expr("my_var");
  auto* var = Const("my_var", ty.f32());
  auto* decl = Decl(Var("b", ty.f32(), ast::StorageClass::kFunction, my_var_a));

  Func("my_func", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           decl,
       },
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(my_var_a), nullptr);
  EXPECT_TRUE(TypeOf(my_var_a)->Is<sem::F32>());
  EXPECT_EQ(StmtOf(my_var_a), decl);
  EXPECT_TRUE(CheckVarUsers(var, {my_var_a}));
  ASSERT_NE(VarOf(my_var_a), nullptr);
  EXPECT_EQ(VarOf(my_var_a)->Declaration(), var);
}

TEST_F(ResolverTest, Expr_Identifier_FunctionVariable) {
  auto* my_var_a = Expr("my_var");
  auto* my_var_b = Expr("my_var");
  auto* assign = create<ast::AssignmentStatement>(my_var_a, my_var_b);

  auto* var = Var("my_var", ty.f32(), ast::StorageClass::kNone);

  Func("my_func", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           assign,
       },
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(my_var_a), nullptr);
  EXPECT_TRUE(TypeOf(my_var_a)->Is<sem::Pointer>());
  EXPECT_TRUE(TypeOf(my_var_a)->As<sem::Pointer>()->type()->Is<sem::F32>());
  EXPECT_EQ(StmtOf(my_var_a), assign);
  ASSERT_NE(TypeOf(my_var_b), nullptr);
  EXPECT_TRUE(TypeOf(my_var_b)->Is<sem::Pointer>());
  EXPECT_TRUE(TypeOf(my_var_b)->As<sem::Pointer>()->type()->Is<sem::F32>());
  EXPECT_EQ(StmtOf(my_var_b), assign);
  EXPECT_TRUE(CheckVarUsers(var, {my_var_a, my_var_b}));
  ASSERT_NE(VarOf(my_var_a), nullptr);
  EXPECT_EQ(VarOf(my_var_a)->Declaration(), var);
  ASSERT_NE(VarOf(my_var_b), nullptr);
  EXPECT_EQ(VarOf(my_var_b)->Declaration(), var);
}

TEST_F(ResolverTest, Expr_Identifier_Function_Ptr) {
  auto* my_var_a = Expr("my_var");
  auto* my_var_b = Expr("my_var");
  auto* assign = create<ast::AssignmentStatement>(my_var_a, my_var_b);

  Func("my_func", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(
               Var("my_var", ty.pointer<f32>(ast::StorageClass::kFunction),
                   ast::StorageClass::kNone)),
           assign,
       },
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(my_var_a), nullptr);
  EXPECT_TRUE(TypeOf(my_var_a)->Is<sem::Pointer>());
  EXPECT_TRUE(TypeOf(my_var_a)->As<sem::Pointer>()->type()->Is<sem::F32>());
  EXPECT_EQ(StmtOf(my_var_a), assign);
  ASSERT_NE(TypeOf(my_var_b), nullptr);
  EXPECT_TRUE(TypeOf(my_var_b)->Is<sem::Pointer>());
  EXPECT_TRUE(TypeOf(my_var_b)->As<sem::Pointer>()->type()->Is<sem::F32>());
  EXPECT_EQ(StmtOf(my_var_b), assign);
}

TEST_F(ResolverTest, Expr_Call_Function) {
  Func("my_func", ast::VariableList{}, ty.f32(),
       ast::StatementList{Return(Expr(0.0f))}, ast::DecorationList{});

  auto* call = Call("my_func");
  WrapInFunction(call);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(call), nullptr);
  EXPECT_TRUE(TypeOf(call)->Is<sem::F32>());
}

TEST_F(ResolverTest, Expr_Identifier_Unknown) {
  auto* a = Expr("a");
  WrapInFunction(a);

  EXPECT_FALSE(r()->Resolve());
}

TEST_F(ResolverTest, Function_Parameters) {
  auto* param_a = Param("a", ty.f32());
  auto* param_b = Param("b", ty.i32());
  auto* param_c = Param("c", ty.u32());

  auto* func = Func("my_func",
                    ast::VariableList{
                        param_a,
                        param_b,
                        param_c,
                    },
                    ty.void_(), {});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  auto* func_sem = Sem().Get(func);
  ASSERT_NE(func_sem, nullptr);
  EXPECT_EQ(func_sem->Parameters().size(), 3u);
  EXPECT_EQ(func_sem->Parameters()[0]->Type(), ty.f32());
  EXPECT_EQ(func_sem->Parameters()[1]->Type(), ty.i32());
  EXPECT_EQ(func_sem->Parameters()[2]->Type(), ty.u32());
  EXPECT_EQ(func_sem->Parameters()[0]->Declaration(), param_a);
  EXPECT_EQ(func_sem->Parameters()[1]->Declaration(), param_b);
  EXPECT_EQ(func_sem->Parameters()[2]->Declaration(), param_c);
}

TEST_F(ResolverTest, Function_RegisterInputOutputVariables) {
  auto* s = Structure("S", {Member("m", ty.u32())},
                      {create<ast::StructBlockDecoration>()});
  auto* a = ty.access(ast::AccessControl::kReadOnly, s);

  auto* in_var = Global("in_var", ty.f32(), ast::StorageClass::kInput);
  auto* out_var = Global("out_var", ty.f32(), ast::StorageClass::kOutput);
  auto* sb_var = Global("sb_var", a, ast::StorageClass::kStorage);
  auto* wg_var = Global("wg_var", ty.f32(), ast::StorageClass::kWorkgroup);
  auto* priv_var = Global("priv_var", ty.f32(), ast::StorageClass::kPrivate);

  auto* func = Func(
      "my_func", ast::VariableList{}, ty.void_(),
      ast::StatementList{
          create<ast::AssignmentStatement>(Expr("out_var"), Expr("in_var")),
          create<ast::AssignmentStatement>(Expr("wg_var"), Expr("wg_var")),
          create<ast::AssignmentStatement>(Expr("sb_var"), Expr("sb_var")),
          create<ast::AssignmentStatement>(Expr("priv_var"), Expr("priv_var")),
      },
      ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  auto* func_sem = Sem().Get(func);
  ASSERT_NE(func_sem, nullptr);
  EXPECT_EQ(func_sem->Parameters().size(), 0u);

  const auto& vars = func_sem->ReferencedModuleVariables();
  ASSERT_EQ(vars.size(), 5u);
  EXPECT_EQ(vars[0]->Declaration(), out_var);
  EXPECT_EQ(vars[1]->Declaration(), in_var);
  EXPECT_EQ(vars[2]->Declaration(), wg_var);
  EXPECT_EQ(vars[3]->Declaration(), sb_var);
  EXPECT_EQ(vars[4]->Declaration(), priv_var);
}

TEST_F(ResolverTest, Function_RegisterInputOutputVariables_SubFunction) {
  auto* s = Structure("S", {Member("m", ty.u32())},
                      {create<ast::StructBlockDecoration>()});
  auto* a = ty.access(ast::AccessControl::kReadOnly, s);

  auto* in_var = Global("in_var", ty.f32(), ast::StorageClass::kInput);
  auto* out_var = Global("out_var", ty.f32(), ast::StorageClass::kOutput);
  auto* sb_var = Global("sb_var", a, ast::StorageClass::kStorage);
  auto* wg_var = Global("wg_var", ty.f32(), ast::StorageClass::kWorkgroup);
  auto* priv_var = Global("priv_var", ty.f32(), ast::StorageClass::kPrivate);

  Func("my_func", ast::VariableList{}, ty.f32(),
       ast::StatementList{
           create<ast::AssignmentStatement>(Expr("out_var"), Expr("in_var")),
           create<ast::AssignmentStatement>(Expr("wg_var"), Expr("wg_var")),
           create<ast::AssignmentStatement>(Expr("sb_var"), Expr("sb_var")),
           create<ast::AssignmentStatement>(Expr("priv_var"), Expr("priv_var")),
           Return(Expr(0.0f))},
       ast::DecorationList{});

  auto* func2 = Func(
      "func", ast::VariableList{}, ty.void_(),
      ast::StatementList{
          create<ast::AssignmentStatement>(Expr("out_var"), Call("my_func")),
      },
      ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  auto* func2_sem = Sem().Get(func2);
  ASSERT_NE(func2_sem, nullptr);
  EXPECT_EQ(func2_sem->Parameters().size(), 0u);

  const auto& vars = func2_sem->ReferencedModuleVariables();
  ASSERT_EQ(vars.size(), 5u);
  EXPECT_EQ(vars[0]->Declaration(), out_var);
  EXPECT_EQ(vars[1]->Declaration(), in_var);
  EXPECT_EQ(vars[2]->Declaration(), wg_var);
  EXPECT_EQ(vars[3]->Declaration(), sb_var);
  EXPECT_EQ(vars[4]->Declaration(), priv_var);
}

TEST_F(ResolverTest, Function_NotRegisterFunctionVariable) {
  auto* var = Var("in_var", ty.f32(), ast::StorageClass::kFunction);
  Global("var", ty.f32(), ast::StorageClass::kFunction);

  auto* func =
      Func("my_func", ast::VariableList{}, ty.void_(),
           ast::StatementList{
               create<ast::VariableDeclStatement>(var),
               create<ast::AssignmentStatement>(Expr("var"), Expr(1.f)),
           },
           ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  auto* func_sem = Sem().Get(func);
  ASSERT_NE(func_sem, nullptr);

  EXPECT_EQ(func_sem->ReferencedModuleVariables().size(), 0u);
}

TEST_F(ResolverTest, Function_ReturnStatements) {
  auto* var = Var("foo", ty.f32(), ast::StorageClass::kFunction);

  auto* ret_1 = create<ast::ReturnStatement>(Expr(1.f));
  auto* ret_foo = create<ast::ReturnStatement>(Expr("foo"));

  auto* func = Func("my_func", ast::VariableList{}, ty.f32(),
                    ast::StatementList{
                        create<ast::VariableDeclStatement>(var),
                        If(Expr(true), Block(ret_1)),
                        ret_foo,
                    },
                    ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  auto* func_sem = Sem().Get(func);
  ASSERT_NE(func_sem, nullptr);
  EXPECT_EQ(func_sem->Parameters().size(), 0u);

  EXPECT_EQ(func_sem->ReturnStatements().size(), 2u);
  EXPECT_EQ(func_sem->ReturnStatements()[0], ret_1);
  EXPECT_EQ(func_sem->ReturnStatements()[1], ret_foo);
}

TEST_F(ResolverTest, Expr_MemberAccessor_Struct) {
  auto* strct = create<ast::Struct>(
      Sym("S"),
      ast::StructMemberList{Member("first_member", ty.i32()),
                            Member("second_member", ty.f32())},
      ast::DecorationList{});

  auto* st = ty.struct_(strct);
  Global("my_struct", st, ast::StorageClass::kInput);

  auto* mem = MemberAccessor("my_struct", "second_member");
  WrapInFunction(mem);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(mem), nullptr);
  ASSERT_TRUE(TypeOf(mem)->Is<sem::Pointer>());

  auto* ptr = TypeOf(mem)->As<sem::Pointer>();
  EXPECT_TRUE(ptr->type()->Is<sem::F32>());
  ASSERT_TRUE(Sem().Get(mem)->Is<sem::StructMemberAccess>());
  EXPECT_EQ(Sem()
                .Get(mem)
                ->As<sem::StructMemberAccess>()
                ->Member()
                ->Declaration()
                ->symbol(),
            Symbols().Get("second_member"));
}

TEST_F(ResolverTest, Expr_MemberAccessor_Struct_Alias) {
  auto* strct = create<ast::Struct>(
      Sym("alias"),
      ast::StructMemberList{Member("first_member", ty.i32()),
                            Member("second_member", ty.f32())},
      ast::DecorationList{});

  auto* st = ty.struct_(strct);
  auto* alias = ty.alias("alias", st);
  Global("my_struct", alias, ast::StorageClass::kInput);

  auto* mem = MemberAccessor("my_struct", "second_member");
  WrapInFunction(mem);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(mem), nullptr);
  ASSERT_TRUE(TypeOf(mem)->Is<sem::Pointer>());

  auto* ptr = TypeOf(mem)->As<sem::Pointer>();
  EXPECT_TRUE(ptr->type()->Is<sem::F32>());
  ASSERT_TRUE(Sem().Get(mem)->Is<sem::StructMemberAccess>());
}

TEST_F(ResolverTest, Expr_MemberAccessor_VectorSwizzle) {
  Global("my_vec", ty.vec3<f32>(), ast::StorageClass::kInput);

  auto* mem = MemberAccessor("my_vec", "xzyw");
  WrapInFunction(mem);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(mem), nullptr);
  ASSERT_TRUE(TypeOf(mem)->Is<sem::Vector>());
  EXPECT_TRUE(TypeOf(mem)->As<sem::Vector>()->type()->Is<sem::F32>());
  EXPECT_EQ(TypeOf(mem)->As<sem::Vector>()->size(), 4u);
  ASSERT_TRUE(Sem().Get(mem)->Is<sem::Swizzle>());
  EXPECT_THAT(Sem().Get(mem)->As<sem::Swizzle>()->Indices(),
              ElementsAre(0, 2, 1, 3));
}

TEST_F(ResolverTest, Expr_MemberAccessor_VectorSwizzle_SingleElement) {
  Global("my_vec", ty.vec3<f32>(), ast::StorageClass::kInput);

  auto* mem = MemberAccessor("my_vec", "b");
  WrapInFunction(mem);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(mem), nullptr);
  ASSERT_TRUE(TypeOf(mem)->Is<sem::Pointer>());

  auto* ptr = TypeOf(mem)->As<sem::Pointer>();
  ASSERT_TRUE(ptr->type()->Is<sem::F32>());
  ASSERT_TRUE(Sem().Get(mem)->Is<sem::Swizzle>());
  EXPECT_THAT(Sem().Get(mem)->As<sem::Swizzle>()->Indices(), ElementsAre(2));
}

TEST_F(ResolverTest, Expr_Accessor_MultiLevel) {
  // struct b {
  //   vec4<f32> foo
  // }
  // struct A {
  //   vec3<struct b> mem
  // }
  // var c : A
  // c.mem[0].foo.yx
  //   -> vec2<f32>
  //
  // MemberAccessor{
  //   MemberAccessor{
  //     ArrayAccessor{
  //       MemberAccessor{
  //         Identifier{c}
  //         Identifier{mem}
  //       }
  //       ScalarConstructor{0}
  //     }
  //     Identifier{foo}
  //   }
  //   Identifier{yx}
  // }
  //

  auto* strctB = create<ast::Struct>(
      Sym("B"), ast::StructMemberList{Member("foo", ty.vec4<f32>())},
      ast::DecorationList{});
  auto* stB = ty.struct_(strctB);

  sem::Vector vecB(stB, 3);
  auto* strctA =
      create<ast::Struct>(Sym("A"), ast::StructMemberList{Member("mem", &vecB)},
                          ast::DecorationList{});

  auto* stA = ty.struct_(strctA);
  Global("c", stA, ast::StorageClass::kInput);

  auto* mem = MemberAccessor(
      MemberAccessor(IndexAccessor(MemberAccessor("c", "mem"), 0), "foo"),
      "yx");
  WrapInFunction(mem);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(mem), nullptr);
  ASSERT_TRUE(TypeOf(mem)->Is<sem::Vector>());
  EXPECT_TRUE(TypeOf(mem)->As<sem::Vector>()->type()->Is<sem::F32>());
  EXPECT_EQ(TypeOf(mem)->As<sem::Vector>()->size(), 2u);
  ASSERT_TRUE(Sem().Get(mem)->Is<sem::Swizzle>());
}

TEST_F(ResolverTest, Expr_MemberAccessor_InBinaryOp) {
  auto* strct = create<ast::Struct>(
      Sym("S"),
      ast::StructMemberList{Member("first_member", ty.f32()),
                            Member("second_member", ty.f32())},
      ast::DecorationList{});

  auto* st = ty.struct_(strct);
  Global("my_struct", st, ast::StorageClass::kInput);

  auto* expr = Add(MemberAccessor("my_struct", "first_member"),
                   MemberAccessor("my_struct", "second_member"));
  WrapInFunction(expr);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(expr), nullptr);
  EXPECT_TRUE(TypeOf(expr)->Is<sem::F32>());
}

namespace ExprBinaryTest {

struct Params {
  ast::BinaryOp op;
  create_type_func_ptr create_lhs_type;
  create_type_func_ptr create_rhs_type;
  create_type_func_ptr create_result_type;
};

static constexpr create_type_func_ptr all_create_type_funcs[] = {
    ty_bool_,       ty_u32,         ty_i32,        ty_f32,
    ty_vec3<bool>,  ty_vec3<i32>,   ty_vec3<u32>,  ty_vec3<f32>,
    ty_mat3x3<i32>, ty_mat3x3<u32>, ty_mat3x3<f32>};

// A list of all valid test cases for 'lhs op rhs', except that for vecN and
// matNxN, we only test N=3.
static constexpr Params all_valid_cases[] = {
    // Logical expressions
    // https://gpuweb.github.io/gpuweb/wgsl.html#logical-expr

    // Binary logical expressions
    Params{Op::kLogicalAnd, ty_bool_, ty_bool_, ty_bool_},
    Params{Op::kLogicalOr, ty_bool_, ty_bool_, ty_bool_},

    Params{Op::kAnd, ty_bool_, ty_bool_, ty_bool_},
    Params{Op::kOr, ty_bool_, ty_bool_, ty_bool_},
    Params{Op::kAnd, ty_vec3<bool>, ty_vec3<bool>, ty_vec3<bool>},
    Params{Op::kOr, ty_vec3<bool>, ty_vec3<bool>, ty_vec3<bool>},

    // Arithmetic expressions
    // https://gpuweb.github.io/gpuweb/wgsl.html#arithmetic-expr

    // Binary arithmetic expressions over scalars
    Params{Op::kAdd, ty_i32, ty_i32, ty_i32},
    Params{Op::kSubtract, ty_i32, ty_i32, ty_i32},
    Params{Op::kMultiply, ty_i32, ty_i32, ty_i32},
    Params{Op::kDivide, ty_i32, ty_i32, ty_i32},
    Params{Op::kModulo, ty_i32, ty_i32, ty_i32},

    Params{Op::kAdd, ty_u32, ty_u32, ty_u32},
    Params{Op::kSubtract, ty_u32, ty_u32, ty_u32},
    Params{Op::kMultiply, ty_u32, ty_u32, ty_u32},
    Params{Op::kDivide, ty_u32, ty_u32, ty_u32},
    Params{Op::kModulo, ty_u32, ty_u32, ty_u32},

    Params{Op::kAdd, ty_f32, ty_f32, ty_f32},
    Params{Op::kSubtract, ty_f32, ty_f32, ty_f32},
    Params{Op::kMultiply, ty_f32, ty_f32, ty_f32},
    Params{Op::kDivide, ty_f32, ty_f32, ty_f32},
    Params{Op::kModulo, ty_f32, ty_f32, ty_f32},

    // Binary arithmetic expressions over vectors
    Params{Op::kAdd, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<i32>},
    Params{Op::kSubtract, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<i32>},
    Params{Op::kMultiply, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<i32>},
    Params{Op::kDivide, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<i32>},
    Params{Op::kModulo, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<i32>},

    Params{Op::kAdd, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>},
    Params{Op::kSubtract, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>},
    Params{Op::kMultiply, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>},
    Params{Op::kDivide, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>},
    Params{Op::kModulo, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>},

    Params{Op::kAdd, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<f32>},
    Params{Op::kSubtract, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<f32>},
    Params{Op::kMultiply, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<f32>},
    Params{Op::kDivide, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<f32>},
    Params{Op::kModulo, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<f32>},

    // Binary arithmetic expressions with mixed scalar, vector, and matrix
    // operands
    Params{Op::kMultiply, ty_vec3<f32>, ty_f32, ty_vec3<f32>},
    Params{Op::kMultiply, ty_f32, ty_vec3<f32>, ty_vec3<f32>},

    Params{Op::kMultiply, ty_mat3x3<f32>, ty_f32, ty_mat3x3<f32>},
    Params{Op::kMultiply, ty_f32, ty_mat3x3<f32>, ty_mat3x3<f32>},

    Params{Op::kMultiply, ty_vec3<f32>, ty_mat3x3<f32>, ty_vec3<f32>},
    Params{Op::kMultiply, ty_mat3x3<f32>, ty_vec3<f32>, ty_vec3<f32>},
    Params{Op::kMultiply, ty_mat3x3<f32>, ty_mat3x3<f32>, ty_mat3x3<f32>},

    // Comparison expressions
    // https://gpuweb.github.io/gpuweb/wgsl.html#comparison-expr

    // Comparisons over scalars
    Params{Op::kEqual, ty_bool_, ty_bool_, ty_bool_},
    Params{Op::kNotEqual, ty_bool_, ty_bool_, ty_bool_},

    Params{Op::kEqual, ty_i32, ty_i32, ty_bool_},
    Params{Op::kNotEqual, ty_i32, ty_i32, ty_bool_},
    Params{Op::kLessThan, ty_i32, ty_i32, ty_bool_},
    Params{Op::kLessThanEqual, ty_i32, ty_i32, ty_bool_},
    Params{Op::kGreaterThan, ty_i32, ty_i32, ty_bool_},
    Params{Op::kGreaterThanEqual, ty_i32, ty_i32, ty_bool_},

    Params{Op::kEqual, ty_u32, ty_u32, ty_bool_},
    Params{Op::kNotEqual, ty_u32, ty_u32, ty_bool_},
    Params{Op::kLessThan, ty_u32, ty_u32, ty_bool_},
    Params{Op::kLessThanEqual, ty_u32, ty_u32, ty_bool_},
    Params{Op::kGreaterThan, ty_u32, ty_u32, ty_bool_},
    Params{Op::kGreaterThanEqual, ty_u32, ty_u32, ty_bool_},

    Params{Op::kEqual, ty_f32, ty_f32, ty_bool_},
    Params{Op::kNotEqual, ty_f32, ty_f32, ty_bool_},
    Params{Op::kLessThan, ty_f32, ty_f32, ty_bool_},
    Params{Op::kLessThanEqual, ty_f32, ty_f32, ty_bool_},
    Params{Op::kGreaterThan, ty_f32, ty_f32, ty_bool_},
    Params{Op::kGreaterThanEqual, ty_f32, ty_f32, ty_bool_},

    // Comparisons over vectors
    Params{Op::kEqual, ty_vec3<bool>, ty_vec3<bool>, ty_vec3<bool>},
    Params{Op::kNotEqual, ty_vec3<bool>, ty_vec3<bool>, ty_vec3<bool>},

    Params{Op::kEqual, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<bool>},
    Params{Op::kNotEqual, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<bool>},
    Params{Op::kLessThan, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<bool>},
    Params{Op::kLessThanEqual, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<bool>},
    Params{Op::kGreaterThan, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<bool>},
    Params{Op::kGreaterThanEqual, ty_vec3<i32>, ty_vec3<i32>, ty_vec3<bool>},

    Params{Op::kEqual, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<bool>},
    Params{Op::kNotEqual, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<bool>},
    Params{Op::kLessThan, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<bool>},
    Params{Op::kLessThanEqual, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<bool>},
    Params{Op::kGreaterThan, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<bool>},
    Params{Op::kGreaterThanEqual, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<bool>},

    Params{Op::kEqual, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<bool>},
    Params{Op::kNotEqual, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<bool>},
    Params{Op::kLessThan, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<bool>},
    Params{Op::kLessThanEqual, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<bool>},
    Params{Op::kGreaterThan, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<bool>},
    Params{Op::kGreaterThanEqual, ty_vec3<f32>, ty_vec3<f32>, ty_vec3<bool>},

    // Bit expressions
    // https://gpuweb.github.io/gpuweb/wgsl.html#bit-expr

    // Binary bitwise operations
    Params{Op::kOr, ty_i32, ty_i32, ty_i32},
    Params{Op::kAnd, ty_i32, ty_i32, ty_i32},
    Params{Op::kXor, ty_i32, ty_i32, ty_i32},

    Params{Op::kOr, ty_u32, ty_u32, ty_u32},
    Params{Op::kAnd, ty_u32, ty_u32, ty_u32},
    Params{Op::kXor, ty_u32, ty_u32, ty_u32},

    // Bit shift expressions
    Params{Op::kShiftLeft, ty_i32, ty_u32, ty_i32},
    Params{Op::kShiftLeft, ty_vec3<i32>, ty_vec3<u32>, ty_vec3<i32>},

    Params{Op::kShiftLeft, ty_u32, ty_u32, ty_u32},
    Params{Op::kShiftLeft, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>},

    Params{Op::kShiftRight, ty_i32, ty_u32, ty_i32},
    Params{Op::kShiftRight, ty_vec3<i32>, ty_vec3<u32>, ty_vec3<i32>},

    Params{Op::kShiftRight, ty_u32, ty_u32, ty_u32},
    Params{Op::kShiftRight, ty_vec3<u32>, ty_vec3<u32>, ty_vec3<u32>}};

using Expr_Binary_Test_Valid = ResolverTestWithParam<Params>;
TEST_P(Expr_Binary_Test_Valid, All) {
  auto& params = GetParam();

  auto* lhs_type = params.create_lhs_type(ty);
  auto* rhs_type = params.create_rhs_type(ty);
  auto* result_type = params.create_result_type(ty);

  std::stringstream ss;
  ss << lhs_type->FriendlyName(Symbols()) << " " << params.op << " "
     << rhs_type->FriendlyName(Symbols());
  SCOPED_TRACE(ss.str());

  Global("lhs", lhs_type, ast::StorageClass::kInput);
  Global("rhs", rhs_type, ast::StorageClass::kInput);

  auto* expr =
      create<ast::BinaryExpression>(params.op, Expr("lhs"), Expr("rhs"));
  WrapInFunction(expr);

  ASSERT_TRUE(r()->Resolve()) << r()->error();
  ASSERT_NE(TypeOf(expr), nullptr);
  ASSERT_TRUE(TypeOf(expr) == result_type);
}
INSTANTIATE_TEST_SUITE_P(ResolverTest,
                         Expr_Binary_Test_Valid,
                         testing::ValuesIn(all_valid_cases));

enum class BinaryExprSide { Left, Right, Both };
using Expr_Binary_Test_WithAlias_Valid =
    ResolverTestWithParam<std::tuple<Params, BinaryExprSide>>;
TEST_P(Expr_Binary_Test_WithAlias_Valid, All) {
  const Params& params = std::get<0>(GetParam());
  BinaryExprSide side = std::get<1>(GetParam());

  auto* lhs_type = params.create_lhs_type(ty);
  auto* rhs_type = params.create_rhs_type(ty);

  std::stringstream ss;
  ss << lhs_type->FriendlyName(Symbols()) << " " << params.op << " "
     << rhs_type->FriendlyName(Symbols());

  // For vectors and matrices, wrap the sub type in an alias
  auto make_alias = [this](sem::Type* type) -> sem::Type* {
    sem::Type* result;
    if (auto* v = type->As<sem::Vector>()) {
      result = create<sem::Vector>(
          create<sem::Alias>(Symbols().New(), v->type()), v->size());
    } else if (auto* m = type->As<sem::Matrix>()) {
      result =
          create<sem::Matrix>(create<sem::Alias>(Symbols().New(), m->type()),
                              m->rows(), m->columns());
    } else {
      result = create<sem::Alias>(Symbols().New(), type);
    }
    return result;
  };

  // Wrap in alias
  if (side == BinaryExprSide::Left || side == BinaryExprSide::Both) {
    lhs_type = make_alias(lhs_type);
  }
  if (side == BinaryExprSide::Right || side == BinaryExprSide::Both) {
    rhs_type = make_alias(rhs_type);
  }

  ss << ", After aliasing: " << lhs_type->FriendlyName(Symbols()) << " "
     << params.op << " " << rhs_type->FriendlyName(Symbols());
  SCOPED_TRACE(ss.str());

  Global("lhs", lhs_type, ast::StorageClass::kInput);
  Global("rhs", rhs_type, ast::StorageClass::kInput);

  auto* expr =
      create<ast::BinaryExpression>(params.op, Expr("lhs"), Expr("rhs"));
  WrapInFunction(expr);

  ASSERT_TRUE(r()->Resolve()) << r()->error();
  ASSERT_NE(TypeOf(expr), nullptr);
  // TODO(amaiorano): Bring this back once we have a way to get the canonical
  // type
  // auto* result_type = params.create_result_type(ty);
  // ASSERT_TRUE(TypeOf(expr) == result_type);
}
INSTANTIATE_TEST_SUITE_P(
    ResolverTest,
    Expr_Binary_Test_WithAlias_Valid,
    testing::Combine(testing::ValuesIn(all_valid_cases),
                     testing::Values(BinaryExprSide::Left,
                                     BinaryExprSide::Right,
                                     BinaryExprSide::Both)));

using Expr_Binary_Test_Invalid =
    ResolverTestWithParam<std::tuple<Params, create_type_func_ptr>>;
TEST_P(Expr_Binary_Test_Invalid, All) {
  const Params& params = std::get<0>(GetParam());
  const create_type_func_ptr& create_type_func = std::get<1>(GetParam());

  // Currently, for most operations, for a given lhs type, there is exactly one
  // rhs type allowed.  The only exception is for multiplication, which allows
  // any permutation of f32, vecN<f32>, and matNxN<f32>. We are fed valid inputs
  // only via `params`, and all possible types via `create_type_func`, so we
  // test invalid combinations by testing every other rhs type, modulo
  // exceptions.

  // Skip valid rhs type
  if (params.create_rhs_type == create_type_func) {
    return;
  }

  auto* lhs_type = params.create_lhs_type(ty);
  auto* rhs_type = create_type_func(ty);

  // Skip exceptions: multiplication of f32, vecN<f32>, and matNxN<f32>
  if (params.op == Op::kMultiply &&
      lhs_type->is_float_scalar_or_vector_or_matrix() &&
      rhs_type->is_float_scalar_or_vector_or_matrix()) {
    return;
  }

  std::stringstream ss;
  ss << lhs_type->FriendlyName(Symbols()) << " " << params.op << " "
     << rhs_type->FriendlyName(Symbols());
  SCOPED_TRACE(ss.str());

  Global("lhs", lhs_type, ast::StorageClass::kInput);
  Global("rhs", rhs_type, ast::StorageClass::kInput);

  auto* expr = create<ast::BinaryExpression>(Source{{12, 34}}, params.op,
                                             Expr("lhs"), Expr("rhs"));
  WrapInFunction(expr);

  ASSERT_FALSE(r()->Resolve());
  ASSERT_EQ(r()->error(),
            "12:34 error: Binary expression operand types are invalid for "
            "this operation: " +
                lhs_type->FriendlyName(Symbols()) + " " +
                FriendlyName(expr->op()) + " " +
                rhs_type->FriendlyName(Symbols()));
}
INSTANTIATE_TEST_SUITE_P(
    ResolverTest,
    Expr_Binary_Test_Invalid,
    testing::Combine(testing::ValuesIn(all_valid_cases),
                     testing::ValuesIn(all_create_type_funcs)));

using Expr_Binary_Test_Invalid_VectorMatrixMultiply =
    ResolverTestWithParam<std::tuple<bool, uint32_t, uint32_t, uint32_t>>;
TEST_P(Expr_Binary_Test_Invalid_VectorMatrixMultiply, All) {
  bool vec_by_mat = std::get<0>(GetParam());
  uint32_t vec_size = std::get<1>(GetParam());
  uint32_t mat_rows = std::get<2>(GetParam());
  uint32_t mat_cols = std::get<3>(GetParam());

  sem::Type* lhs_type;
  sem::Type* rhs_type;
  sem::Type* result_type;
  bool is_valid_expr;

  if (vec_by_mat) {
    lhs_type = create<sem::Vector>(ty.f32(), vec_size);
    rhs_type = create<sem::Matrix>(ty.f32(), mat_rows, mat_cols);
    result_type = create<sem::Vector>(ty.f32(), mat_cols);
    is_valid_expr = vec_size == mat_rows;
  } else {
    lhs_type = create<sem::Matrix>(ty.f32(), mat_rows, mat_cols);
    rhs_type = create<sem::Vector>(ty.f32(), vec_size);
    result_type = create<sem::Vector>(ty.f32(), mat_rows);
    is_valid_expr = vec_size == mat_cols;
  }

  Global("lhs", lhs_type, ast::StorageClass::kInput);
  Global("rhs", rhs_type, ast::StorageClass::kInput);

  auto* expr = Mul(Source{{12, 34}}, Expr("lhs"), Expr("rhs"));
  WrapInFunction(expr);

  if (is_valid_expr) {
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    ASSERT_TRUE(TypeOf(expr) == result_type);
  } else {
    ASSERT_FALSE(r()->Resolve());
    ASSERT_EQ(r()->error(),
              "12:34 error: Binary expression operand types are invalid for "
              "this operation: " +
                  lhs_type->FriendlyName(Symbols()) + " " +
                  FriendlyName(expr->op()) + " " +
                  rhs_type->FriendlyName(Symbols()));
  }
}
auto all_dimension_values = testing::Values(2u, 3u, 4u);
INSTANTIATE_TEST_SUITE_P(ResolverTest,
                         Expr_Binary_Test_Invalid_VectorMatrixMultiply,
                         testing::Combine(testing::Values(true, false),
                                          all_dimension_values,
                                          all_dimension_values,
                                          all_dimension_values));

using Expr_Binary_Test_Invalid_MatrixMatrixMultiply =
    ResolverTestWithParam<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>>;
TEST_P(Expr_Binary_Test_Invalid_MatrixMatrixMultiply, All) {
  uint32_t lhs_mat_rows = std::get<0>(GetParam());
  uint32_t lhs_mat_cols = std::get<1>(GetParam());
  uint32_t rhs_mat_rows = std::get<2>(GetParam());
  uint32_t rhs_mat_cols = std::get<3>(GetParam());

  auto* lhs_type = create<sem::Matrix>(ty.f32(), lhs_mat_rows, lhs_mat_cols);
  auto* rhs_type = create<sem::Matrix>(ty.f32(), rhs_mat_rows, rhs_mat_cols);
  auto* result_type = create<sem::Matrix>(ty.f32(), lhs_mat_rows, rhs_mat_cols);

  Global("lhs", lhs_type, ast::StorageClass::kInput);
  Global("rhs", rhs_type, ast::StorageClass::kInput);

  auto* expr = Mul(Source{{12, 34}}, Expr("lhs"), Expr("rhs"));
  WrapInFunction(expr);

  bool is_valid_expr = lhs_mat_cols == rhs_mat_rows;
  if (is_valid_expr) {
    ASSERT_TRUE(r()->Resolve()) << r()->error();
    ASSERT_TRUE(TypeOf(expr) == result_type);
  } else {
    ASSERT_FALSE(r()->Resolve());
    ASSERT_EQ(r()->error(),
              "12:34 error: Binary expression operand types are invalid for "
              "this operation: " +
                  lhs_type->FriendlyName(Symbols()) + " " +
                  FriendlyName(expr->op()) + " " +
                  rhs_type->FriendlyName(Symbols()));
  }
}
INSTANTIATE_TEST_SUITE_P(ResolverTest,
                         Expr_Binary_Test_Invalid_MatrixMatrixMultiply,
                         testing::Combine(all_dimension_values,
                                          all_dimension_values,
                                          all_dimension_values,
                                          all_dimension_values));

}  // namespace ExprBinaryTest

using UnaryOpExpressionTest = ResolverTestWithParam<ast::UnaryOp>;
TEST_P(UnaryOpExpressionTest, Expr_UnaryOp) {
  auto op = GetParam();

  Global("ident", ty.vec4<f32>(), ast::StorageClass::kInput);
  auto* der = create<ast::UnaryOpExpression>(op, Expr("ident"));
  WrapInFunction(der);

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  ASSERT_NE(TypeOf(der), nullptr);
  ASSERT_TRUE(TypeOf(der)->Is<sem::Vector>());
  EXPECT_TRUE(TypeOf(der)->As<sem::Vector>()->type()->Is<sem::F32>());
  EXPECT_EQ(TypeOf(der)->As<sem::Vector>()->size(), 4u);
}
INSTANTIATE_TEST_SUITE_P(ResolverTest,
                         UnaryOpExpressionTest,
                         testing::Values(ast::UnaryOp::kNegation,
                                         ast::UnaryOp::kNot));

TEST_F(ResolverTest, StorageClass_SetsIfMissing) {
  auto* var = Var("var", ty.i32(), ast::StorageClass::kNone);

  auto* stmt = create<ast::VariableDeclStatement>(var);
  Func("func", ast::VariableList{}, ty.void_(), ast::StatementList{stmt},
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  EXPECT_EQ(Sem().Get(var)->StorageClass(), ast::StorageClass::kFunction);
}

TEST_F(ResolverTest, StorageClass_DoesNotSetOnConst) {
  auto* var = Const("var", ty.i32());
  auto* stmt = create<ast::VariableDeclStatement>(var);
  Func("func", ast::VariableList{}, ty.void_(), ast::StatementList{stmt},
       ast::DecorationList{});

  EXPECT_TRUE(r()->Resolve()) << r()->error();

  EXPECT_EQ(Sem().Get(var)->StorageClass(), ast::StorageClass::kNone);
}

TEST_F(ResolverTest, Function_EntryPoints_StageDecoration) {
  // fn b() {}
  // fn c() { b(); }
  // fn a() { c(); }
  // fn ep_1() { a(); b(); }
  // fn ep_2() { c();}
  //
  // c -> {ep_1, ep_2}
  // a -> {ep_1}
  // b -> {ep_1, ep_2}
  // ep_1 -> {}
  // ep_2 -> {}

  Global("first", ty.f32(), ast::StorageClass::kPrivate);
  Global("second", ty.f32(), ast::StorageClass::kPrivate);
  Global("call_a", ty.f32(), ast::StorageClass::kPrivate);
  Global("call_b", ty.f32(), ast::StorageClass::kPrivate);
  Global("call_c", ty.f32(), ast::StorageClass::kPrivate);

  ast::VariableList params;
  auto* func_b =
      Func("b", params, ty.f32(), ast::StatementList{Return(Expr(0.0f))},
           ast::DecorationList{});
  auto* func_c = Func("c", params, ty.f32(),
                      ast::StatementList{create<ast::AssignmentStatement>(
                                             Expr("second"), Call("b")),
                                         Return(Expr(0.0f))},
                      ast::DecorationList{});

  auto* func_a = Func("a", params, ty.f32(),
                      ast::StatementList{create<ast::AssignmentStatement>(
                                             Expr("first"), Call("c")),
                                         Return(Expr(0.0f))},
                      ast::DecorationList{});

  auto* ep_1 =
      Func("ep_1", params, ty.void_(),
           ast::StatementList{
               create<ast::AssignmentStatement>(Expr("call_a"), Call("a")),
               create<ast::AssignmentStatement>(Expr("call_b"), Call("b")),
           },
           ast::DecorationList{
               create<ast::StageDecoration>(ast::PipelineStage::kVertex),
           });

  auto* ep_2 =
      Func("ep_2", params, ty.void_(),
           ast::StatementList{
               create<ast::AssignmentStatement>(Expr("call_c"), Call("c")),
           },
           ast::DecorationList{
               create<ast::StageDecoration>(ast::PipelineStage::kVertex),
           });

  ASSERT_TRUE(r()->Resolve()) << r()->error();

  auto* func_b_sem = Sem().Get(func_b);
  auto* func_a_sem = Sem().Get(func_a);
  auto* func_c_sem = Sem().Get(func_c);
  auto* ep_1_sem = Sem().Get(ep_1);
  auto* ep_2_sem = Sem().Get(ep_2);
  ASSERT_NE(func_b_sem, nullptr);
  ASSERT_NE(func_a_sem, nullptr);
  ASSERT_NE(func_c_sem, nullptr);
  ASSERT_NE(ep_1_sem, nullptr);
  ASSERT_NE(ep_2_sem, nullptr);

  EXPECT_EQ(func_b_sem->Parameters().size(), 0u);
  EXPECT_EQ(func_a_sem->Parameters().size(), 0u);
  EXPECT_EQ(func_c_sem->Parameters().size(), 0u);

  const auto& b_eps = func_b_sem->AncestorEntryPoints();
  ASSERT_EQ(2u, b_eps.size());
  EXPECT_EQ(Symbols().Register("ep_1"), b_eps[0]);
  EXPECT_EQ(Symbols().Register("ep_2"), b_eps[1]);

  const auto& a_eps = func_a_sem->AncestorEntryPoints();
  ASSERT_EQ(1u, a_eps.size());
  EXPECT_EQ(Symbols().Register("ep_1"), a_eps[0]);

  const auto& c_eps = func_c_sem->AncestorEntryPoints();
  ASSERT_EQ(2u, c_eps.size());
  EXPECT_EQ(Symbols().Register("ep_1"), c_eps[0]);
  EXPECT_EQ(Symbols().Register("ep_2"), c_eps[1]);

  EXPECT_TRUE(ep_1_sem->AncestorEntryPoints().empty());
  EXPECT_TRUE(ep_2_sem->AncestorEntryPoints().empty());
}

// Check for linear-time traversal of functions reachable from entry points.
// See: crbug.com/tint/245
TEST_F(ResolverTest, Function_EntryPoints_LinearTime) {
  // fn lNa() { }
  // fn lNb() { }
  // ...
  // fn l2a() { l3a(); l3b(); }
  // fn l2b() { l3a(); l3b(); }
  // fn l1a() { l2a(); l2b(); }
  // fn l1b() { l2a(); l2b(); }
  // fn main() { l1a(); l1b(); }

  static constexpr int levels = 64;

  auto fn_a = [](int level) { return "l" + std::to_string(level + 1) + "a"; };
  auto fn_b = [](int level) { return "l" + std::to_string(level + 1) + "b"; };

  Func(fn_a(levels), {}, ty.void_(), {}, {});
  Func(fn_b(levels), {}, ty.void_(), {}, {});

  for (int i = levels - 1; i >= 0; i--) {
    Func(fn_a(i), {}, ty.void_(),
         {
             create<ast::CallStatement>(Call(fn_a(i + 1))),
             create<ast::CallStatement>(Call(fn_b(i + 1))),
         },
         {});
    Func(fn_b(i), {}, ty.void_(),
         {
             create<ast::CallStatement>(Call(fn_a(i + 1))),
             create<ast::CallStatement>(Call(fn_b(i + 1))),
         },
         {});
  }

  Func("main", {}, ty.void_(),
       {
           create<ast::CallStatement>(Call(fn_a(0))),
           create<ast::CallStatement>(Call(fn_b(0))),
       },
       {
           create<ast::StageDecoration>(ast::PipelineStage::kVertex),
       });

  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

// Test for crbug.com/tint/728
TEST_F(ResolverTest, ASTNodesAreReached) {
  Structure("A", {Member("x", ty.array<f32, 4>(4))});
  Structure("B", {Member("x", ty.array<f32, 4>(4))});
  ASSERT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverTest, ASTNodeNotReached) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder builder;
        builder.Expr("1");
        Resolver(&builder).Resolve();
      },
      "internal compiler error: AST node 'tint::ast::IdentifierExpression' was "
      "not reached by the resolver");
}

TEST_F(ResolverTest, ASTNodeReachedTwice) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder builder;
        auto* expr = builder.Expr("1");
        auto* usesExprTwice = builder.Add(expr, expr);
        builder.Global("g", builder.ty.i32(), ast::StorageClass::kPrivate,
                       usesExprTwice);
        Resolver(&builder).Resolve();
      },
      "internal compiler error: AST node 'tint::ast::IdentifierExpression' was "
      "encountered twice in the same AST of a Program");
}

}  // namespace
}  // namespace resolver
}  // namespace tint
