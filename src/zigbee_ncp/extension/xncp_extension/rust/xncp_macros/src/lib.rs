//! `#[xncp_command(id, feature = ...)]` registers a handler into `XNCP_COMMANDS` and its
//! feature into `XNCP_FEATURES`. `xncp_feature!(...)` registers a feature on its own.
use proc_macro::TokenStream;
use proc_macro2::TokenStream as TokenStream2;
use quote::{format_ident, quote};
use syn::parse::{Parse, ParseStream};
use syn::{parse_macro_input, Error, Expr, Ident, ItemFn, LitBool, Path, Token};

struct CommandArgs {
    id: Expr,
    feature: Path,
    allow_duplicate: bool,
}

impl Parse for CommandArgs {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let id = input.parse()?;
        let mut feature = None;
        let mut allow_duplicate = false;

        while !input.is_empty() {
            input.parse::<Token![,]>()?;
            let key: Ident = input.parse()?;
            input.parse::<Token![=]>()?;
            match key.to_string().as_str() {
                "feature" => feature = Some(input.parse()?),
                "allow_duplicate" => allow_duplicate = input.parse::<LitBool>()?.value,
                _ => return Err(Error::new(key.span(), "unknown argument")),
            }
        }

        Ok(Self {
            id,
            feature: feature.ok_or_else(|| input.error("missing `feature = ...`"))?,
            allow_duplicate,
        })
    }
}

fn register_feature(feature: &Path, reg: &Ident, allow_duplicate: bool) -> TokenStream2 {
    let name = feature
        .segments
        .last()
        .unwrap()
        .ident
        .to_string()
        .to_uppercase();

    // A second claim of a feature fails: in the same crate as a duplicate definition, in
    // another crate as a duplicate symbol at link time
    let claim = (!allow_duplicate).then(|| {
        let symbol = format_ident!("__XNCP_FEATURE_CLAIM_{}", name);
        quote! {
            #[no_mangle]
            static #symbol: u8 = 0;
        }
    });

    quote! {
        #claim

        const _: u32 = #feature.bit();

        #[::xncp_core::distributed_slice(::xncp_core::XNCP_FEATURES)]
        static #reg: ::xncp_core::XncpFeature = #feature;
    }
}

#[proc_macro_attribute]
pub fn xncp_command(attr: TokenStream, item: TokenStream) -> TokenStream {
    let CommandArgs {
        id,
        feature,
        allow_duplicate,
    } = parse_macro_input!(attr as CommandArgs);
    let func = parse_macro_input!(item as ItemFn);
    let name = &func.sig.ident;
    let upper = name.to_string().to_uppercase();
    let reg = format_ident!("__XNCP_CMD_{}", upper);
    let feature_reg = register_feature(
        &feature,
        &format_ident!("__XNCP_CMD_FEATURE_{}", upper),
        allow_duplicate,
    );

    quote! {
        #func

        #[::xncp_core::distributed_slice(::xncp_core::XNCP_COMMANDS)]
        static #reg: ::xncp_core::XncpCommandDef = ::xncp_core::XncpCommandDef {
            command_id: #id,
            handler: #name,
        };

        #feature_reg
    }
    .into()
}

#[proc_macro]
pub fn xncp_feature(input: TokenStream) -> TokenStream {
    let feature = parse_macro_input!(input as Path);
    let reg = format_ident!(
        "__XNCP_FEATURE_{}",
        feature
            .segments
            .last()
            .unwrap()
            .ident
            .to_string()
            .to_uppercase()
    );
    register_feature(&feature, &reg, false).into()
}
