﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

using System;
using Microsoft.Quantum.Simulation.Common;
using Microsoft.Quantum.Simulation.Core;

namespace Microsoft.Quantum.Simulation.QuantumProcessor
{

    public partial class QuantumProcessorDispatcher<TProcessor>
    {
        public class QuantumProcessorDispatcherR1Frac : Quantum.Intrinsic.R1Frac
        {
            private QuantumProcessorDispatcher Simulator { get; }

            public QuantumProcessorDispatcherR1Frac(QuantumProcessorDispatcher m) : base(m)
            {
                this.Simulator = m;
            }

            public override Func<(long, long, Qubit), QVoid> Body => (_args) =>
            {
                (long num, long denom, Qubit q1) = _args;
                (long numNew, long denomNew) = CommonUtils.Reduce(num, denom);
                Simulator.QuantumProcessor.R1Frac(numNew, denomNew, q1);
                return QVoid.Instance;
            };

            public override Func<(long, long, Qubit), QVoid> AdjointBody => (_args) =>
            {
                (long num, long denom, Qubit q1) = _args;
                return this.Body.Invoke((-num, denom, q1));
            };

            public override Func<(IQArray<Qubit>, (long, long, Qubit)), QVoid> ControlledBody => (_args) =>
            {
                (IQArray<Qubit> ctrls, (long num, long denom, Qubit q1)) = _args;
                (long numNew, long denomNew) = CommonUtils.Reduce(num, denom);

                if ((ctrls == null) || (ctrls.Count == 0))
                {
                    Simulator.QuantumProcessor.R1Frac(numNew, denomNew, q1);
                }
                else
                {
                    Simulator.QuantumProcessor.ControlledR1Frac(ctrls, numNew, denomNew, q1);
                }

                return QVoid.Instance;
            };

            public override Func<(IQArray<Qubit>, (long, long, Qubit)), QVoid> ControlledAdjointBody => (_args) =>
            {
                (IQArray<Qubit> ctrls, (long num, long denom, Qubit q1)) = _args;
                return this.ControlledBody.Invoke((ctrls, (-num, denom, q1)));
            };
        }
    }
}
