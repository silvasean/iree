# RUN: %PYTHON %s | iree-dialects-opt | FileCheck --enable-var-scope --dump-input-filter=all %s

from typing import List
from mlir.dialects.iree_pydm.importer.test_util import *


# CHECK-LABEL: @assign_free_var_not_arg
# CHECK: %[[CST:.*]] = iree_pydm.constant 1
# CHECK: %[[BOXED:.*]] = iree_pydm.box %[[CST]] : !iree_pydm.integer -> !iree_pydm.object<!iree_pydm.integer>
# CHECK: iree_pydm.store_free_var "x", %[[BOXED]]
@test_import_global
def assign_free_var_not_arg():
  x = 1
  return x
