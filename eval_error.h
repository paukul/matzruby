/*
 * included by eval.c
 */

#define SET_CURRENT_SOURCE() ((void)0)

void
ruby_set_current_source(void)
{
    if (ruby_current_node) {
	ruby_sourcefile = ruby_current_node->nd_file;
	ruby_sourceline = nd_line(ruby_current_node);
    }
}

static void
warn_printf(const char *fmt, ...)
{
    char buf[BUFSIZ];
    va_list args;

    va_init_list(args, fmt);
    vsnprintf(buf, BUFSIZ, fmt, args);
    va_end(args);
    rb_write_error(buf);
}

#define warn_print(x) rb_write_error(x)
#define warn_print2(x,l) rb_write_error2(x,l)

static void
error_pos(void)
{
    ruby_set_current_source();
    if (ruby_sourcefile) {
	if (ruby_sourceline == 0) {
	    warn_printf("%s", ruby_sourcefile);
	}
	else if (rb_frame_callee()) {
	    warn_printf("%s:%d:in `%s'", ruby_sourcefile, ruby_sourceline,
			rb_id2name(rb_frame_callee()));
	}
	else {
	    warn_printf("%s:%d", ruby_sourcefile, ruby_sourceline);
	}
    }
}

static VALUE
get_backtrace(VALUE info)
{
    if (NIL_P(info))
	return Qnil;
    info = rb_funcall(info, rb_intern("backtrace"), 0);
    if (NIL_P(info))
	return Qnil;
    return rb_check_array_type(info);
}

static void
set_backtrace(VALUE info, VALUE bt)
{
    rb_funcall(info, rb_intern("set_backtrace"), 1, bt);
}

static void
error_print(void)
{
    VALUE errat = Qnil;		/* OK */
    volatile VALUE eclass, e;
    char *einfo;
    long elen;

    if (NIL_P(GET_THREAD()->errinfo))
	return;

    PUSH_TAG();
    if (EXEC_TAG() == 0) {
	errat = get_backtrace(GET_THREAD()->errinfo);
    }
    else {
	errat = Qnil;
    }
    if (EXEC_TAG())
	goto error;
    if (NIL_P(errat)) {
	ruby_set_current_source();
	if (ruby_sourcefile)
	    warn_printf("%s:%d", ruby_sourcefile, ruby_sourceline);
	else
	    warn_printf("%d", ruby_sourceline);
    }
    else if (RARRAY_LEN(errat) == 0) {
	error_pos();
    }
    else {
	VALUE mesg = RARRAY_PTR(errat)[0];

	if (NIL_P(mesg))
	    error_pos();
	else {
	    warn_print2(RSTRING_PTR(mesg), RSTRING_LEN(mesg));
	}
    }

    eclass = CLASS_OF(GET_THREAD()->errinfo);
    if (EXEC_TAG() == 0) {
	e = rb_funcall(GET_THREAD()->errinfo, rb_intern("message"), 0, 0);
	StringValue(e);
	einfo = RSTRING_PTR(e);
	elen = RSTRING_LEN(e);
    }
    else {
	einfo = "";
	elen = 0;
    }
    if (EXEC_TAG())
	goto error;
    if (eclass == rb_eRuntimeError && elen == 0) {
	warn_print(": unhandled exception\n");
    }
    else {
	VALUE epath;

	epath = rb_class_name(eclass);
	if (elen == 0) {
	    warn_print(": ");
	    warn_print2(RSTRING_PTR(epath), RSTRING_LEN(epath));
	    warn_print("\n");
	}
	else {
	    char *tail = 0;
	    long len = elen;

	    if (RSTRING_PTR(epath)[0] == '#')
		epath = 0;
	    if (tail = memchr(einfo, '\n', elen)) {
		len = tail - einfo;
		tail++;		/* skip newline */
	    }
	    warn_print(": ");
	    warn_print2(einfo, len);
	    if (epath) {
		warn_print(" (");
		warn_print2(RSTRING_PTR(epath), RSTRING_LEN(epath));
		warn_print(")\n");
	    }
	    if (tail) {
		warn_print2(tail, elen - len - 1);
	    }
	}
    }

    if (!NIL_P(errat)) {
	long i;
	long len = RARRAY_LEN(errat);
	VALUE *ptr = RARRAY_PTR(errat);
        int skip = eclass == rb_eSysStackError;
	
#define TRACE_MAX (TRACE_HEAD+TRACE_TAIL+5)
#define TRACE_HEAD 8
#define TRACE_TAIL 5

	for (i = 1; i < len; i++) {
	    if (TYPE(ptr[i]) == T_STRING) {
		warn_printf("\tfrom %s\n", RSTRING_PTR(ptr[i]));
	    }
	    if (skip && i == TRACE_HEAD && len > TRACE_MAX) {
		warn_printf("\t ... %ld levels...\n",
			    len - TRACE_HEAD - TRACE_TAIL);
		i = len - TRACE_TAIL;
	    }
	}
    }
  error:
    POP_TAG();
}

void
print_undef(VALUE klass, ID id)
{
    rb_name_error(id, "undefined method `%s' for %s `%s'",
		  rb_id2name(id),
		  (TYPE(klass) == T_MODULE) ? "module" : "class",
		  rb_class2name(klass));
}

VALUE exception_error;
VALUE sysstack_error;

static int
sysexit_status(VALUE err)
{
    VALUE st = rb_iv_get(err, "status");
    return NUM2INT(st);
}

static int
error_handle(int ex)
{
    int status = EXIT_FAILURE;
    rb_thread_t *th = GET_THREAD();

    if (thread_set_raised(th))
	return EXIT_FAILURE;
    switch (ex & TAG_MASK) {
      case 0:
	status = EXIT_SUCCESS;
	break;

      case TAG_RETURN:
	error_pos();
	warn_print(": unexpected return\n");
	break;
      case TAG_NEXT:
	error_pos();
	warn_print(": unexpected next\n");
	break;
      case TAG_BREAK:
	error_pos();
	warn_print(": unexpected break\n");
	break;
      case TAG_REDO:
	error_pos();
	warn_print(": unexpected redo\n");
	break;
      case TAG_RETRY:
	error_pos();
	warn_print(": retry outside of rescue clause\n");
	break;
      case TAG_THROW:
	/* TODO: fix me */
	error_pos();
	warn_printf(": unexpected throw\n");
	break;
      case TAG_RAISE:
      case TAG_FATAL: {
	VALUE errinfo = GET_THREAD()->errinfo;
	if (rb_obj_is_kind_of(errinfo, rb_eSystemExit)) {
	    status = sysexit_status(errinfo);
	}
	else if (rb_obj_is_instance_of(errinfo, rb_eSignal)) {
	    /* no message when exiting by signal */
	}
	else {
	    error_print();
	}
	break;
      }
      default:
	rb_bug("Unknown longjmp status %d", ex);
	break;
    }
    thread_reset_raised(th);
    return status;
}
