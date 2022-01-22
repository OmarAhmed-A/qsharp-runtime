// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use crate::c64;

use ndarray::linalg::Dot;
use ndarray::{Array2, ArrayBase, Axis, Data, Ix2, NewAxis, Zip};
use ndarray_linalg::{assert_close_l2, Eig, EigVals, Eigh, Lapack, Scalar};
use thiserror::Error;

use super::HasDagger;

#[derive(Error, Debug)]
pub enum ExpmError {
    #[error(transparent)]
    Linalg(#[from] ndarray_linalg::error::LinalgError),
    #[error("Broadcast error")]
    Broadcast,
}

/// Trait marking types that support the matrix exponential operation.
pub trait Expm
where
    Self: Sized,
{
    type Error: std::error::Error;
    fn expm(&self) -> Result<Self, Self::Error>;
}

// FIXME: Generalize to avoid having to redefine for f64, c64.
// impl<A, S> Expm for ArrayBase<S, Ix2>
// where
//     A: Scalar + Lapack,
//     S: Data<Elem = A>,
//     Self: Eig,
//     <Self as Eig>::EigVal: Iterator,
//     <<Self as Eig>::EigVal as Iterator>::Item: Scalar,
//     <Self as Eig>::EigVec: HasDagger
// ArrayBase<Data<Elem = (A as Scalar)::Complex>, Ix2>: HasDagger
impl Expm for Array2<c64> {
    type Error = ExpmError;

    fn expm(&self) -> Result<Self, Self::Error> {
        let (eigvals, eigvecs) = self.eig()?;
        let eigvals = eigvals.map(|e| e.exp());

        // Rescale right vectors by eigvals.
        let left = eigvecs.to_owned();
        let right = eigvals
            .broadcast(eigvecs.dim())
            .map_or(Err(ExpmError::Broadcast), Result::Ok)?
            .to_owned()
            * eigvecs;

        Ok(left.dag().dot(&right))
    }
}
