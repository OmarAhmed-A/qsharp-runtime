// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use crate::c64;
use ndarray::{Array2, ArrayView2};

/// Represents types that have hermitian conjugates (e.g.: $A^\dagger$ for
/// a matrix $A$ is defined as the complex conjugate transpose of $A$,
/// $(A^\dagger)\_{ij} = A\_{ji}^*$).
pub trait HasDagger {
    /// The type of the hermitian conjugate.
    type Output;

    /// Returns the hermitian conjugate (colloquially, the dagger) of a
    /// borrowed reference as a new copy.
    ///
    /// For most types implementing this trait, the hermitian conjugate
    /// is represented by the conjugate transpose.
    fn dag(&self) -> Self::Output;
}

impl HasDagger for Array2<c64> {
    type Output = Self;

    fn dag(&self) -> Self {
        self.t().map(|element| element.conj())
    }
}

impl HasDagger for ArrayView2<'_, c64> {
    type Output = Array2<c64>;

    fn dag(&self) -> Self::Output {
        self.t().map(|element| element.conj())
    }
}

/// Represent types that can be conjugated by 2-dimensional arrays; that is,
/// as $UXU^{\dagger}$.
pub trait ConjBy {
    /// Conjugates this value by a given matrix, returning a copy.
    fn conjugate_by(&self, op: &ArrayView2<c64>) -> Self;
}

impl ConjBy for Array2<c64> {
    fn conjugate_by(&self, op: &ArrayView2<c64>) -> Self {
        op.dot(self).dot(&op.dag())
    }
}
