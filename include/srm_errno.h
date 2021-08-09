#ifndef __srm_errno_h__
#define __srm_errno_h__

/* SRM: System Resource Monitor */


#define SRM_OK                    0x0                                 /* sucess. */

#define SRM_ERR_NO_BASE                         0x2                  /*  NO OF BASE FOR ERROR */
#define SRM_VERSION_MISMATCH_ERR        (SRM_ERR_NO_BASE)           /* Plugin version does not match the SRM version. */
#define SRM_ALREADY_REG_ERR             (SRM_ERR_NO_BASE + 0x1)     /* Plugin already register on SRM system. */
#define SRM_REGISTER_ERR                (SRM_ERR_NO_BASE + 0x2)     /* Wrong registration information. */
#define SRM_ALLOC_SPACE_ERR             (SRM_ERR_NO_BASE + 0x3)     /* Failed to request memory space. */
#define SRM_CREATE_OBJ_ERR              (SRM_ERR_NO_BASE + 0x4)     /* Builting C-object occurred wrong. */
#define SRM_ILLEGAL_CALL_ERR            (SRM_ERR_NO_BASE + 0x5)     /* Operation is illegal when was called function. */
#define SRM_NOT_ALLOWED_ERR             (SRM_ERR_NO_BASE + 0x6)     /* Operation is not allow. */
#define SRM_PARAMETER_ERR               (SRM_ERR_NO_BASE + 0x7)     /* Parameter wrong. */
#define SRM_DATA_BAD_ERR                (SRM_ERR_NO_BASE + 0x8)     /* Data occurred wrong. */


#endif  /* __srm_errno_h__ */

