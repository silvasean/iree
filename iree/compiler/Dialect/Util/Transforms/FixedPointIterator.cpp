// Copyright 2021 The IREE Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace Util {
namespace {

// Dynamic pass which runs a sub-pipeline to a fixed point or a maximum
// iteration count.
//
// There is no direct coupling between this iterator and the contained passes.
// Indirectly, at the start of each iteration, this pass will set the
// "iree.fixedpoint.converged" unit attribute on the root operation. If it is
// still there when the sub-pipeline is complete, it will be removed and
// iteration terminates. If a sub-pass removes it, then iteration will
// continue.
class FixedPointIteratorPass
    : public PassWrapper<FixedPointIteratorPass, OperationPass<void>> {
 public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(FixedPointIteratorPass)

  FixedPointIteratorPass() = default;
  FixedPointIteratorPass(const FixedPointIteratorPass &other)
      : PassWrapper(other) {}
  FixedPointIteratorPass(OpPassManager pipeline);

 private:
  LogicalResult initializeOptions(StringRef options) override;
  void getDependentDialects(DialectRegistry &registry) const override;
  StringRef getArgument() const override {
    return "iree-util-fixed-point-iterator";
  }
  StringRef getDescription() const override {
    return "Iterates a sub-pipeline to a fixed point";
  }

  void runOnOperation() override;

  Option<OpPassManager> pipeline{
      *this, "pipeline", llvm::cl::desc("Pipeline to run to a fixed point")};
  Option<int> maxIterations{*this, "max-iterations",
                            llvm::cl::desc("Maximum number of iterations"),
                            llvm::cl::init(10)};
};

FixedPointIteratorPass::FixedPointIteratorPass(OpPassManager pipeline) {
  this->pipeline.setValue(pipeline);
}

LogicalResult FixedPointIteratorPass::initializeOptions(StringRef options) {
  if (failed(Pass::initializeOptions(options))) return failure();
  return success();
}

void FixedPointIteratorPass::getDependentDialects(
    DialectRegistry &registry) const {
  pipeline.getValue().getDependentDialects(registry);
}

void FixedPointIteratorPass::runOnOperation() {
  MLIRContext *context = &getContext();
  StringAttr markerName = StringAttr::get(context, "iree.fixedpoint.iteration");
  StringAttr modifiedName =
      StringAttr::get(context, "iree.fixedpoint.modified");

  if (getOperation()->hasAttr(markerName)) {
    emitError(getOperation()->getLoc())
        << "nested fixed point pipelines not supported";
    return signalPassFailure();
  }

  for (int i = 0; i < maxIterations; ++i) {
    getOperation()->setAttr(markerName,
                            IntegerAttr::get(IndexType::get(context), i));
    getOperation()->removeAttr(modifiedName);
    if (failed(runPipeline(pipeline.getValue(), getOperation()))) {
      return signalPassFailure();
    }

    if (!getOperation()->hasAttr(modifiedName)) {
      // Normal exit.
      getOperation()->removeAttr(markerName);
      return;
    }
  }

  // Abnormal exit - iteration count exceeded.
  emitError(getOperation()->getLoc())
      << "maximum iteration count exceeded in fixed point pipeline";
  return signalPassFailure();
}

}  // namespace

std::unique_ptr<OperationPass<void>> createFixedPointIteratorPass(
    OpPassManager pipeline) {
  static PassRegistration<FixedPointIteratorPass> pass;
  return std::make_unique<FixedPointIteratorPass>(std::move(pipeline));
}

}  // namespace Util
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir
