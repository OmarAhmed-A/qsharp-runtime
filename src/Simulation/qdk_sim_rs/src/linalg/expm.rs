// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use crate::c64;

use ndarray::Array2;
use ndarray_linalg::Eig;
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
///
/// Note that this trait requires the `expm` crate feature to be enabled.
pub trait Expm
where
    Self: Sized,
{
    /// The type of errors that can result from matrix exponential operations.
    type Error: std::error::Error;

    /// Returns the exponential of a given matrix.
    fn expm(&self) -> Result<Self, Self::Error>;
}

// TODO: Generalize to avoid having to redefine for f64, c64.
// Previous attempt at constraints:
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
        // Our strategy is to use the eigenvalue decomposition of our
        // input to compute the matrix exponential. This is not the most
        // numerically stable strategy in general, but works well for small
        // matrices.
        //
        // In the future, we may want to consider using a more stable strategy
        // such as the Padé approximant approach when inputs are large.

        let (eigvals, eigvecs) = self.eig()?;
        let eigvals = eigvals.map(|e| e.exp());

        let eigvecs = eigvecs.t();

        // Rescale right vectors by eigvals.
        let left = eigvals
            .broadcast(eigvecs.dim())
            .map_or(Err(ExpmError::Broadcast), Result::Ok)?
            .to_owned()
            * eigvecs;
        let right = eigvecs.dag();

        Ok(left.dot(&right))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ndarray_linalg::assert_close_l2;
    use num_traits::Zero;

    #[test]
    fn test_expm_works_for_small_diagonal_matrices() -> Result<(), ExpmError> {
        let argument = c64::new(0.0, -1.0)
            * (array![
                [c64::new(-3.0, 0.0), c64::zero()],
                [c64::zero(), c64::new(5.0, 0.0)]
            ]);
        let u = argument.expm()?;
        assert_close_l2!(
            &u,
            &array![
                [c64::new(-0.9899925, 0.14112001), c64::zero()],
                [c64::zero(), c64::new(0.28366219, 0.95892427)]
            ],
            1e-6
        );
        Ok(())
    }

    #[test]
    fn test_expm_works_for_small_off_diagonal_matrices() -> Result<(), ExpmError> {
        let argument = array![
            [c64::zero(), c64::new(0.0, 1.234)],
            [c64::new(0.0, 1.234), c64::zero()]
        ];
        let u = argument.expm()?;
        assert_close_l2!(
            &u,
            &array![
                [c64::new(0.33046511, 0.0), c64::new(0.0, 0.94381821)],
                [c64::new(0.0, 0.94381821), c64::new(0.33046511, 0.0)]
            ],
            1e-6
        );
        Ok(())
    }
}
