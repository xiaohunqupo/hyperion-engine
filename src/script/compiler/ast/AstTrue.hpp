#ifndef AST_TRUE_HPP
#define AST_TRUE_HPP

#include <script/compiler/ast/AstConstant.hpp>

namespace hyperion::compiler {

class AstTrue : public AstConstant {
public:
    AstTrue(const SourceLocation &location);

    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool IsNumber() const override;
    virtual hyperion::Int32 IntValue() const override;
    virtual hyperion::Float32 FloatValue() const override;
    virtual SymbolTypePtr_t GetExprType() const override;

    virtual std::shared_ptr<AstConstant> HandleOperator(Operators op_type, const AstConstant *right) const override;

private:
    Pointer<AstTrue> CloneImpl() const
    {
        return Pointer<AstTrue>(new AstTrue(
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
