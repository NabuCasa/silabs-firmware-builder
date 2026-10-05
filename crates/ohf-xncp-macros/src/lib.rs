//! `#[xncp_command(id)]` registers a handler into `XNCP_COMMANDS`.
use proc_macro::TokenStream;
use quote::{format_ident, quote};
use syn::{parse_macro_input, Expr, ItemFn};

#[proc_macro_attribute]
pub fn xncp_command(attr: TokenStream, item: TokenStream) -> TokenStream {
    let id = parse_macro_input!(attr as Expr);
    let func = parse_macro_input!(item as ItemFn);
    let name = &func.sig.ident;
    let reg = format_ident!("__XNCP_CMD_{}", name.to_string().to_uppercase());

    quote! {
        #func

        #[::ohf_xncp::distributed_slice(::ohf_xncp::XNCP_COMMANDS)]
        static #reg: ::ohf_xncp::XncpCommandDef = ::ohf_xncp::XncpCommandDef {
            command_id: #id,
            handler: #name,
        };
    }
    .into()
}
