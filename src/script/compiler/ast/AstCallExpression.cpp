#include <script/compiler/ast/AstCallExpression.hpp>
#include <script/compiler/Compiler.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstNewExpression.hpp>
#include <script/compiler/SemanticAnalyzer.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>
#include <system/Debug.hpp>
#include <util/UTF8.hpp>

#include <limits>
#include <iostream>

namespace hyperion::compiler {

AstCallExpression::AstCallExpression(
    const std::shared_ptr<AstExpression> &target,
    const std::vector<std::shared_ptr<AstArgument>> &args,
    bool insert_self,
    const SourceLocation &location
) : AstExpression(location, ACCESS_MODE_LOAD),
    m_target(target),
    m_args(args),
    m_insert_self(insert_self),
    m_return_type(BuiltinTypes::UNDEFINED),
    m_is_method_call(false)
{
    for (auto &arg : m_args) {
        AssertThrow(arg != nullptr);
    }
}

void AstCallExpression::Visit(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);
    m_target->Visit(visitor, mod);

    SymbolTypePtr_t target_type = m_target->GetExprType();
    AssertThrow(target_type != nullptr);

    m_substituted_args = m_args;

    if (m_insert_self) {
        // if the target is a member expression,
        // place it as 'self' argument to the call
        /*if (auto *target_mem = dynamic_cast<const AstMember *>(m_target.get())) {
            m_is_method_call = true;

            const auto self_target = CloneAstNode(target_mem->GetTarget());
            AssertThrow(self_target != nullptr);

            std::shared_ptr<AstArgument> self_arg(new AstArgument(
                self_target,
                false,
                true,
                "self",
                self_target->GetLocation()
            ));
            
            // insert at front
            m_substituted_args.insert(m_substituted_args.begin(), self_arg);
        } else if (dynamic_cast<const AstCallExpression *>(m_target.get())) {
            m_is_method_call = true;

            const auto self_target = m_target;//CloneAstNode(m_target);
            AssertThrow(self_target != nullptr);

            std::shared_ptr<AstArgument> self_arg(new AstArgument(
                self_target,
                false,
                true,
                "self",
                self_target->GetLocation()
            ));
            
            // insert at front
            m_substituted_args.insert(m_substituted_args.begin(), self_arg);
        }*/

        /*if (const auto *new_target = dynamic_cast<const AstNewExpression *>(m_target.get())) {

        } else */
        
        if (const auto *left_target = m_target->GetTarget()) {
            m_is_method_call = true;

            const auto self_target = CloneAstNode(left_target);
            AssertThrow(self_target != nullptr);

            std::shared_ptr<AstArgument> self_arg(new AstArgument(
                self_target,
                false,
                true,
                "self",
                self_target->GetLocation()
            ));
            
            // insert at front
            m_substituted_args.insert(m_substituted_args.begin(), self_arg);
        }
    }

    // allow unboxing
    SymbolTypePtr_t unboxed_type = target_type;

    if (target_type->GetTypeClass() == TYPE_GENERIC_INSTANCE) {
        if (target_type->IsBoxedType()) {
            unboxed_type = target_type->GetGenericInstanceInfo().m_generic_args[0].m_type;
        }
    }

    AssertThrow(unboxed_type != nullptr);

    SymbolTypePtr_t call_member_type;
    std::string call_member_name;

    if ((call_member_type = unboxed_type->FindPrototypeMember("$invoke"))) {
        call_member_name = "$invoke";
    } else if ((call_member_type = unboxed_type->FindPrototypeMember("$construct"))) {
        call_member_name = "$construct";
    }

    if (call_member_type != nullptr) {
        m_is_method_call = true;

        // if (call_member_name == "$invoke") {
            // closure objects have a self parameter for the '$invoke' call.
            std::shared_ptr<AstArgument> self_arg(new AstArgument(
                m_target,
                false,
                false,
                "__closure_self",
                m_target->GetLocation()
            ));
            
            // insert at front
            m_substituted_args.insert(m_substituted_args.begin(), self_arg);
        // }

        m_target.reset(new AstMember(
            call_member_name,
            CloneAstNode(m_target),
            m_location
        ));
        
        AssertThrow(m_target != nullptr);
        m_target->Visit(visitor, mod);

        unboxed_type = call_member_type;
        AssertThrow(unboxed_type != nullptr);
    }

    // visit each argument
    for (auto &arg : m_substituted_args) {
        AssertThrow(arg != nullptr);

        // note, visit in current module rather than module access
        // this is used so that we can call functions from separate modules,
        // yet still pass variables from the local module.
        arg->Visit(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
    }

    FunctionTypeSignature_t substituted = SemanticAnalyzer::Helpers::SubstituteFunctionArgs(
        visitor,
        mod,
        unboxed_type,
        m_substituted_args,
        m_location
    );

    if (substituted.first != nullptr) {
        m_return_type = substituted.first;
        // change args to be newly ordered vector
        m_substituted_args = substituted.second;
    } else {
        // not a function type
        visitor->GetCompilationUnit()->GetErrorList().AddError(CompilerError(
            LEVEL_ERROR,
            Msg_not_a_function,
            m_location,
            target_type->GetName()
        ));
    }
}

std::unique_ptr<Buildable> AstCallExpression::Build(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    std::unique_ptr<BytecodeChunk> chunk = BytecodeUtil::Make<BytecodeChunk>();

    // build arguments
    chunk->Append(Compiler::BuildArgumentsStart(
        visitor,
        mod,
        m_substituted_args
    ));

    chunk->Append(Compiler::BuildCall(
        visitor,
        mod,
        m_target,
        static_cast<uint8_t>(m_substituted_args.size())
    ));

    chunk->Append(Compiler::BuildArgumentsEnd(
        visitor,
        mod,
        m_substituted_args.size()
    ));

    return std::move(chunk);
}

void AstCallExpression::Optimize(AstVisitor *visitor, Module *mod)
{
    AssertThrow(m_target != nullptr);

    m_target->Optimize(visitor, mod);

    // optimize each argument
    for (auto &arg : m_substituted_args) {
        if (arg != nullptr) {
            arg->Optimize(visitor, visitor->GetCompilationUnit()->GetCurrentModule());
        }
    }
}

Pointer<AstStatement> AstCallExpression::Clone() const
{
    return CloneImpl();
}

Tribool AstCallExpression::IsTrue() const
{
    // cannot deduce if return value is true
    return Tribool::Indeterminate();
}

bool AstCallExpression::MayHaveSideEffects() const
{
    // assume a function call has side effects
    // maybe we could detect this later
    return true;
}

SymbolTypePtr_t AstCallExpression::GetExprType() const
{
    AssertThrow(m_return_type != nullptr);
    return m_return_type;
}

AstExpression *AstCallExpression::GetTarget() const
{
    if (m_target != nullptr) {
        if (auto *nested_target = m_target->GetTarget()) {
            return nested_target;
        }

        return m_target.get();
    }

    return AstExpression::GetTarget();
}

} // namespace hyperion::compiler
