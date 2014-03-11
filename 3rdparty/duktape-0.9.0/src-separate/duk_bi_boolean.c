/*
 *  Boolean built-ins
 */

#include "duk_internal.h"

/* Shared helper to provide toString() and valueOf().  Checks 'this', gets
 * the primitive value to stack top, and optionally coerces with ToString().
 */
int duk_bi_boolean_prototype_tostring_shared(duk_context *ctx) {
	duk_tval *tv;
	duk_hobject *h;
	int coerce_tostring = duk_get_magic(ctx);

	/* FIXME: there is room to use a shared helper here, many built-ins
	 * check the 'this' type, and if it's an object, check its class,
	 * then get its internal value, etc.
	 */

	duk_push_this(ctx);
	tv = duk_get_tval(ctx, -1);
	DUK_ASSERT(tv != NULL);

	if (DUK_TVAL_IS_BOOLEAN(tv)) {
		goto type_ok;
	} else if (DUK_TVAL_IS_OBJECT(tv)) {
		h = DUK_TVAL_GET_OBJECT(tv);
		DUK_ASSERT(h != NULL);

		if (DUK_HOBJECT_GET_CLASS_NUMBER(h) == DUK_HOBJECT_CLASS_BOOLEAN) {
			duk_get_prop_stridx(ctx, -1, DUK_STRIDX_INT_VALUE);
			DUK_ASSERT(duk_is_boolean(ctx, -1));
			goto type_ok;
		}
	}

	return DUK_RET_TYPE_ERROR;

 type_ok:
	if (coerce_tostring) {
		duk_to_string(ctx, -1);
	}
	return 1;
}

int duk_bi_boolean_constructor(duk_context *ctx) {
	duk_hobject *h_this;

	duk_to_boolean(ctx, 0);

	if (duk_is_constructor_call(ctx)) {
		/* FIXME: helper; rely on Boolean.prototype as being non-writable, non-configurable */
		duk_push_this(ctx);
		h_this = duk_get_hobject(ctx, -1);
		DUK_ASSERT(h_this != NULL);
		DUK_ASSERT(h_this->prototype == ((duk_hthread *) ctx)->builtins[DUK_BIDX_BOOLEAN_PROTOTYPE]);

		DUK_HOBJECT_SET_CLASS_NUMBER(h_this, DUK_HOBJECT_CLASS_BOOLEAN);

		duk_dup(ctx, 0);  /* -> [ val obj val ] */
		duk_def_prop_stridx(ctx, -2, DUK_STRIDX_INT_VALUE, DUK_PROPDESC_FLAGS_NONE);  /* FIXME: proper flags? */
	}  /* unbalanced stack */

	return 1;
}

