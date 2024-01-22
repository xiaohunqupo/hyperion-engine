#ifndef AST_OBJECT_HPP
#define AST_OBJECT_HPP

#include <script/compiler/ast/AstExpression.hpp>
#include <core/lib/DynArray.hpp>

namespace hyperion::compiler {

class AstObject : public AstExpression
{
public:
    AstObject(const SymbolTypePtr_t &symbol_type, const SourceLocation &location);
    virtual ~AstObject() override = default;

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual RC<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual HashCode GetHashCode() const override
    {
        HashCode hc = AstExpression::GetHashCode().Add(TypeName<AstObject>());
        hc.Add(m_symbol_type != nullptr ? m_symbol_type->GetHashCode() : HashCode());

        return hc;
    }

private:
    struct ObjectMember
    {
        String              name;
        SymbolTypePtr_t     type;
        RC<AstExpression>   value;
    };

    SymbolTypePtr_t     m_symbol_type;

    // set while analyzing
    Array<ObjectMember> m_members;

    RC<AstObject> CloneImpl() const
    {
        return RC<AstObject>(new AstObject(
            m_symbol_type,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
