// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

use ndarray::array;
use num_traits::Zero;
use qdk_sim::{c64, linalg::Expm};

pub fn main() {
    let hamiltonian = array![
        [c64::new(-3.0, 0.0), c64::zero()],
        [c64::zero(), c64::new(5.0, 0.0)]
    ];
    let argument = c64::new(0.0, -1.0) * hamiltonian;
    let u = argument.expm();
    print!("exp(-1j * H) == {:?}", u);
}
