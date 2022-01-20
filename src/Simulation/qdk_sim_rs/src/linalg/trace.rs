// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use ndarray::Array2;
use num_traits::Zero;

/// Represents types for which the trace can be computed.
pub trait Trace {
    /// The type returned by the trace.
    type Output;

    /// The trace (typically, the sum of the eigenvalues,
    /// or the sum of the diagonal elements $\sum_i A_{ii}$).
    ///
    /// # Example
    /// ```
    /// // TODO
    /// ```
    fn trace(self) -> Self::Output;
}

impl<T: Clone + Zero> Trace for Array2<T> {
    type Output = T;

    fn trace(self) -> Self::Output {
        self.diag().sum()
    }
}

impl<T: Clone + Zero> Trace for &Array2<T> {
    type Output = T;

    fn trace(self) -> Self::Output {
        self.diag().sum()
    }
}
