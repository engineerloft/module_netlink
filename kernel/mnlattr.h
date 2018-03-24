#ifndef __MNLATTR_H__
#define __MNLATTR_H__

/* attributes (variables):
 * the index in this enum is used as a reference for the type,
 * userspace application has to indicate the corresponding type
 * the policy is used for security considerations 
 */
enum {
	DOC_EXMPL_A_UNSPEC,
	DOC_EXMPL_A_TS_NESTED,
	__DOC_EXMPL_A_MAX,
};
#define DOC_EXMPL_A_MAX (__DOC_EXMPL_A_MAX - 1)

enum {
	DOC_EXMPL_A_TS_NESTED_UNSPEC,
	DOC_EXMPL_A_TS_NESTED_TYPE,
	DOC_EXMPL_A_TS_NESTED_SEC,
	DOC_EXMPL_A_TS_NESTED_NSEC,
	DOC_EXMPL_A_TS_NESTED_SEQ,
	DOC_EXMPL_A_TS_NESTED_VALID,
	__DOC_EXMPL_A_TS_NESTED_MAX,
};
#define DOC_EXMPL_A_TS_NESTED_MAX (__DOC_EXMPL_A_TS_NESTED_MAX - 1)

#endif
