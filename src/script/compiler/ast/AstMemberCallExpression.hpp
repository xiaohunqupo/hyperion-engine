#ifndef AST_MEMBER_CALL_EXPRESSION_HPP
#define AST_MEMBER_CALL_EXPRESSION_HPP

#include <script/compiler/ast/AstMember.hpp>
#include <script/compiler/ast/AstArgumentList.hpp>

namespace hyperion::compiler {

class AstMemberCallExpression : public AstMember {
public:
    AstMemberCallExpression(
        const std::string &field_name,
        const std::shared_ptr<AstExpression> &target,
        const std::shared_ptr<AstArgumentList> &arguments,
        const SourceLocation &location
    );
    virtual ~AstMemberCallExpression() = default;
    
    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    virtual Tribool IsTrue() const override;
    virtual bool MayHaveSideEffects() const override;
    virtual SymbolTypePtr_t GetExprType() const override;
  
    virtual const AstExpression *GetValueOf() const override;
    virtual const AstExpression *GetDeepValueOf() const override;
    virtual AstExpression *GetTarget() const override;

protected:
    std::shared_ptr<AstArgumentList> m_arguments;

    // set while analyzing
    std::vector<std::shared_ptr<AstArgument>> m_substituted_args;
    SymbolTypePtr_t m_return_type;

    Pointer<AstMemberCallExpression> CloneImpl() const
    {
        return Pointer<AstMemberCallExpression>(new AstMemberCallExpression(
            m_field_name,
            CloneAstNode(m_target),
            CloneAstNode(m_arguments),
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
