#include <script/compiler/ast/AstNil.hpp>
#include <script/compiler/ast/AstInteger.hpp>
#include <script/compiler/ast/AstFalse.hpp>
#include <script/compiler/ast/AstTrue.hpp>
#include <script/compiler/AstVisitor.hpp>
#include <script/compiler/Keywords.hpp>

#include <script/compiler/type-system/BuiltinTypes.hpp>

#include <script/compiler/emit/BytecodeChunk.hpp>
#include <script/compiler/emit/BytecodeUtil.hpp>

#include <script/Instructions.hpp>

namespace hyperion {
namespace compiler {

AstNil::AstNil(const SourceLocation &location)
    : AstConstant(location)
{
}

std::unique_ptr<Buildable> AstNil::Build(AstVisitor *visitor, Module *mod)
{
    // get active register
    uint8_t rp = visitor->GetCompilationUnit()->GetInstructionStream().GetCurrentRegister();

    return BytecodeUtil::Make<ConstNull>(rp);
}

Pointer<AstStatement> AstNil::Clone() const
{
    return CloneImpl();
}

Tribool AstNil::IsTrue() const
{
    return Tribool::False();
}

bool AstNil::IsNumber() const
{
    return false;
}

hyperion::aint32 AstNil::IntValue() const
{
    return 0;
}

hyperion::afloat32 AstNil::FloatValue() const
{
    return 0.0f;
}

SymbolTypePtr_t AstNil::GetSymbolType() const
{
    return BuiltinTypes::NULL_TYPE;
}

std::shared_ptr<AstConstant> AstNil::HandleOperator(Operators op_type, AstConstant *right) const
{
    switch (op_type) {
        case OP_logical_and:
            // logical operations still work, so that we can do
            // things like testing for null in an if statement.
            return std::shared_ptr<AstFalse>(new AstFalse(m_location));

        case OP_logical_or:
            if (!right->IsNumber()) {
                // this operator is valid to compare against null
                if (dynamic_cast<AstNil*>(right) != nullptr) {
                    return std::shared_ptr<AstFalse>(new AstFalse(m_location));
                }
                return nullptr;
            }

            return std::shared_ptr<AstInteger>(new AstInteger(
                right->IntValue(),
                m_location
            ));

        case OP_equals:
            if (dynamic_cast<AstNil*>(right) != nullptr) {
                // only another null value should be equal
                return std::shared_ptr<AstTrue>(new AstTrue(m_location));
            }
            // other values never equal to null
            return std::shared_ptr<AstFalse>(new AstFalse(m_location));

        case OP_logical_not:
            return std::shared_ptr<AstTrue>(new AstTrue(m_location));

        default:
            return nullptr;
    }
}

} // namespace compiler
} // namespace hyperion
