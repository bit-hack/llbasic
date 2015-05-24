#include "llb_pt.h"
#include "llb_file.h"
#include "llb_string.h"
#include "llb_module.h"
#include "llb_context.h"
#include "llb_backend_cpp.h"

llb_backend_cpp_t::llb_backend_cpp_t()
    : path_("out.cpp")
    , out_()
    , cxt_(nullptr)
    , indent_(0)
{
}

void llb_backend_cpp_t::get_dependant_passes(llb_pass_manager_t & manager) {

    manager.schedule( llb_pass_type_t::e_pass_linker );
}

bool llb_backend_cpp_t::run(llb_context_t & modules, llb_fail_t & fail) {

    cxt_ = &modules;

    try {
        emit_header();
        for_each_module([&](pt_module_t & module){ emit_decls  (module); });
        for_each_module([&](pt_module_t & module){ emit_globals(module); });
        for_each_module([&](pt_module_t & module){ emit_impls  (module); });
    }
    catch (llb_fail_t & thrown) {
        fail = thrown;
        return false;
    }

    file_writer_t out;
    if (!out.open(path_.c_str()))
        return false;
    
    out.write(out_.to_string());
    return true;
}

void llb_backend_cpp_t::for_each_module(std::function<void(pt_module_t &)> func) {

    assert(cxt_);
    pt_t & pt = cxt_->pt_;
    for (auto & node : pt.stack_) {

        assert(node->is_a<pt_module_t>());
        pt_module_t * module = node->upcast<pt_module_t>();
        func(*module);
    }
}

void llb_backend_cpp_t::emit_header() {

    out_.println( "// auto generated by llbasic compiler" );
    out_.println("#include \"llb_runtime.h\"");
    out_.new_line();
}

void llb_backend_cpp_t::emit_decl(pt_function_decl_t & func) {
    
    if (!func.body_) {
        out_.print("extern ");
    }
    else {
        out_.print("static ");
    }

    out_.print("%0 ", { func.ret_type_.get_string() });
    out_.print("%0(", { func.name_.get_string() });

    bool first = true;
    for (auto & arg_node : func.args_) {
        if (!first)
            out_.print(", ");
        first = false;
        assert(arg_node->is_a<pt_decl_var_t>());
        pt_decl_var_t * var = arg_node->upcast<pt_decl_var_t>();
        out_.print("%0 %1", { var->type_.get_string(), var->name_.get_string() });
    }
    out_.print(")");
}

void llb_backend_cpp_t::emit_decls(pt_module_t & module) {

    if (module.functions_.size() == 0)
        return;
    out_.println("// decls from '%0'", { module.name_ });
    for (auto & node : module.functions_) {

        assert(node->is_a<pt_function_decl_t>());
        pt_function_decl_t * decl = node->upcast<pt_function_decl_t>();

        emit_decl(*decl);
        out_.print(";");
        out_.new_line();
    }
    out_.new_line();
}

void llb_backend_cpp_t::emit_global(pt_decl_var_t & global) {

    out_.print("static %0 %1", { global.type_.get_string(), global.name_.get_string() });
}

void llb_backend_cpp_t::emit_globals(pt_module_t & module) {

    if (module.globals_.size() == 0)
        return;
    out_.println("// globals from '%0'", { module.name_ });
    for (auto & node : module.globals_) {

        assert(node->is_a<pt_decl_var_t>());
        pt_decl_var_t * decl = node->upcast<pt_decl_var_t>();

        emit_global(*decl);
        out_.print(";");
        out_.new_line();
    }
    out_.new_line();
}

void llb_backend_cpp_t::emit_body(pt_function_body_t & body) {

    indent_ += 1;

    for (auto & local : body.ext_.locals_) {
        
        pt_decl_var_t * var = local->upcast<pt_decl_var_t>();
        assert(var != nullptr);

        out_.fill(' ', indent_ * 2);
        out_.print("%0 %1;", { var->type_.get_string(), var->name_.get_string() } );
        out_.new_line();
    }

    for (auto & node : body.stmt_) {
        out_.fill(' ', indent_*2);
        node->accept(*this);
        out_.new_line();
    }
    indent_ -= 1;
}

void llb_backend_cpp_t::visit(pt_stmt_t & stmt) {
    llb_pt_walker_t::visit(stmt);
    out_.put_char(';');
}

void llb_backend_cpp_t::emit_locals(pt_function_body_t & body) {

    for (auto & local : body.ext_.locals_) {
        
    }
}

void llb_backend_cpp_t::emit_impls(pt_module_t & module) {

    if (module.functions_.size() == 0)
        return;
    for (auto & node : module.functions_) {

        assert(node->is_a<pt_function_decl_t>());
        pt_function_decl_t * decl = node->upcast<pt_function_decl_t>();
        assert(decl);

        if (!decl->body_.get())
            continue;

        pt_function_body_t * body = decl->body_->upcast<pt_function_body_t>();
        assert(body);

        emit_decl(*decl);
        out_.print(" {");
        out_.new_line();

        emit_locals(*body);
        emit_body(*body);

        out_.print("}");
        out_.new_line();
        out_.new_line();
    }
}

void llb_backend_cpp_t::visit(pt_op_bin_t & n) {

    bool bracket = true;
    if (n.operator_.type_ == tok_chr_assign)
        bracket = false;

    if (bracket)
        out_.put_char('(');
    n.lhs_->accept(*this);
    out_.print(llb_token_t::get_type_symbol(n.operator_.type_));
    n.rhs_->accept(*this);
    if (bracket)
        out_.put_char(')');
}

void llb_backend_cpp_t::visit(pt_identifier_t & n) {

    out_.print(n.name_.get_string());
}

void llb_backend_cpp_t::visit(pt_literal_t & n) {

    switch (n.value_.type_) {
    case (llb_token_type_t::tok_lit_float) :
        out_.print("%0f", { n.value_.get_float() });
        break;
    case (llb_token_type_t::tok_lit_integer) :
        out_.print("%0", { n.value_.get_int() });
        break;
    case (llb_token_type_t::tok_lit_string) :
        out_.print("%0", { n.value_.get_string() });
        break;
    default:
        assert(!"unknown token type");
    }
}

void llb_backend_cpp_t::visit(pt_return_t & n) {

    out_.print("return ");
    if (n.expr_.get()) {
        n.expr_->accept(*this);
    }
    out_.print(";");
}
