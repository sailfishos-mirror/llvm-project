// RUN: not llvm-mc -triple amdgcn-amd-amdhsa -mcpu=gfx1200 -filetype=null %s 2>&1 | FileCheck --check-prefix=ERR %s

.text

.amdhsa_kernel complete
// user_sgpr_count range: 0-16
// ERR: error: too many user SGPRs enabled
  .amdhsa_user_sgpr_count 17

  .amdhsa_next_free_vgpr 9
  .amdhsa_next_free_sgpr 27
.end_amdhsa_kernel

