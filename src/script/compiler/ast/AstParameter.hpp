#ifndef AST_PARAMETER_HPP
#define AST_PARAMETER_HPP

#include <script/compiler/ast/AstDeclaration.hpp>
#include <script/compiler/ast/AstPrototypeSpecification.hpp>
#include <script/compiler/ast/AstExpression.hpp>

namespace hyperion::compiler {

class AstParameter : public AstDeclaration {
public:
    AstParameter(const std::string &name,
        const std::shared_ptr<AstPrototypeSpecification> &type_spec,
        const std::shared_ptr<AstExpression> &default_param,
        bool is_variadic,
        bool is_const,
        const SourceLocation &location);
    virtual ~AstParameter() = default;

    const std::shared_ptr<AstExpression> &GetDefaultValue() const
        { return m_default_param; }
    void SetDefaultValue(const std::shared_ptr<AstExpression> &default_param)
        { m_default_param = default_param; }

    virtual void Visit(AstVisitor *visitor, Module *mod) override;
    virtual std::unique_ptr<Buildable> Build(AstVisitor *visitor, Module *mod) override;
    virtual void Optimize(AstVisitor *visitor, Module *mod) override;
    
    virtual Pointer<AstStatement> Clone() const override;

    bool IsVariadic() const { return m_is_variadic; }
    bool IsConst() const { return m_is_const; }

    bool IsGenericParam() const                   { return m_is_generic_param; }
    void SetIsGenericParam(bool is_generic_param) { m_is_generic_param = is_generic_param; }

    // used by AstTemplateExpression
    const std::shared_ptr<AstPrototypeSpecification> &GetPrototypeSpecification() const
        { return m_type_spec; } 
    void SetPrototypeSpecification(const std::shared_ptr<AstPrototypeSpecification> &type_spec)
        { m_type_spec = type_spec; }

private:
    std::shared_ptr<AstPrototypeSpecification> m_type_spec;
    std::shared_ptr<AstExpression> m_default_param;
    bool m_is_variadic;
    bool m_is_const;
    bool m_is_generic_param;

    Pointer<AstParameter> CloneImpl() const
    {
        return Pointer<AstParameter>(new AstParameter(
            m_name,
            CloneAstNode(m_type_spec),
            CloneAstNode(m_default_param),
            m_is_variadic,
            m_is_const,
            m_location
        ));
    }
};

} // namespace hyperion::compiler

#endif
