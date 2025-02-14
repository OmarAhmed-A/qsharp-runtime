// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <algorithm>
#include <cassert>
#include <set>
#include <sstream>

#include "tracer.hpp"
#include "TracerInternal.hpp"

namespace Microsoft
{
namespace Quantum
{
    thread_local std::shared_ptr<CTracer> tracer = nullptr;
    std::shared_ptr<CTracer> CreateTracer(int preferredLayerDuration)
    {
        tracer = std::make_shared<CTracer>(preferredLayerDuration);
        return tracer;
    }
    std::shared_ptr<CTracer> CreateTracer(int preferredLayerDuration,
                                          const std::unordered_map<OpId, std::string>& opNames)
    {
        tracer = std::make_shared<CTracer>(preferredLayerDuration, opNames);
        return tracer;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::LaterLayerOf
    //------------------------------------------------------------------------------------------------------------------
    /*static*/ LayerId CTracer::LaterLayerOf(LayerId l1, LayerId l2)
    {
        return std::max(l1, l2);
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer's IRuntimeDriver implementation
    //------------------------------------------------------------------------------------------------------------------
    QubitIdType CTracer::AllocateQubit()
    {
        size_t qubit = qubits.size();
        qubits.emplace_back(QubitState{});
        return static_cast<QubitIdType>(qubit);
    }

    void CTracer::ReleaseQubit(QubitIdType /*qubit*/)
    {
        // nothing for now
    }

    // TODO: what would be meaningful information we could printout for a qubit?
    std::string CTracer::QubitToString(QubitIdType q)
    {
        size_t qubitIndex        = static_cast<size_t>(q);
        const QubitState& qstate = this->UseQubit(q);

        std::stringstream str(std::to_string(qubitIndex));
        str << " last used in layer " << qstate.layer << "(pending zero ops: " << qstate.pendingZeroDurationOps.size()
            << ")";
        return str.str();
    }

    void CTracer::ReleaseResult(Result /*result*/)
    {
        // nothing to do, we don't allocate results on measurement [yet]
    }

    // Although the tracer should never compare results or get their values, it still has to implement UseZero and
    // UseOne methods as they are invoked by the QIR initialization.
    Result CTracer::UseZero()
    {
        return reinterpret_cast<Result>(INVALID);
    }

    Result CTracer::UseOne()
    {
        return reinterpret_cast<Result>(INVALID);
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::CreateNewLayer
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::CreateNewLayer(Duration minRequiredDuration)
    {
        // Create a new layer for the operation.
        Time layerStartTime = 0;
        if (!this->metricsByLayer.empty())
        {
            const Layer& lastLayer = this->metricsByLayer.back();
            layerStartTime         = lastLayer.startTime + lastLayer.duration;
        }
        this->metricsByLayer.emplace_back(
            Layer{layerStartTime, std::max(this->preferredLayerDuration, minRequiredDuration)});

        return (LayerId)(this->metricsByLayer.size() - 1);
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::GetEffectiveFence
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::GetEffectiveFence() const
    {
        return CTracer::LaterLayerOf(this->globalBarrier, this->latestConditionalFence);
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::FindLayerToInsertOperationInto
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::FindLayerToInsertOperationInto(QubitIdType q, Duration opDuration) const
    {
        const QubitState& qstate = this->UseQubit(q);

        LayerId layerToInsertInto = REQUESTNEW;

        const LayerId barrier = this->GetEffectiveFence();
        const LayerId firstLayerAfterBarrier =
            (barrier == INVALID ? (this->metricsByLayer.empty() ? REQUESTNEW : 0)
                                : (((size_t)(barrier + 1) == this->metricsByLayer.size()) ? REQUESTNEW : barrier + 1));

        LayerId candidate = CTracer::LaterLayerOf(qstate.layer, firstLayerAfterBarrier);
        assert(candidate != INVALID);

        if (candidate != REQUESTNEW)
        {
            // Find the earliest layer that the operation fits in by duration
            const Layer& candidateLayer = this->metricsByLayer[(size_t)candidate];
            const Time lastUsedTime     = std::max(qstate.lastUsedTime, candidateLayer.startTime);
            if (lastUsedTime + opDuration <= candidateLayer.startTime + candidateLayer.duration)
            {
                layerToInsertInto = candidate;
            }
            else
            {
                for (candidate += 1; (size_t)candidate < this->metricsByLayer.size(); ++candidate)
                {
                    if (opDuration <= this->metricsByLayer[(size_t)candidate].duration)
                    {
                        layerToInsertInto = candidate;
                        break;
                    }
                }
            }
        }

        return layerToInsertInto;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::AddOperationToLayer
    //------------------------------------------------------------------------------------------------------------------
    void CTracer::AddOperationToLayer(OpId id, LayerId layer)
    {
        assert((size_t)layer < this->metricsByLayer.size());
        assert(this->metricsByLayer[(size_t)layer].barrierId == -1 && "Should not add operations to barriers");

        this->metricsByLayer[(size_t)layer].operations[id] += 1;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::UpdateQubitState
    //------------------------------------------------------------------------------------------------------------------
    void CTracer::UpdateQubitState(QubitIdType q, LayerId layer, Duration opDuration)
    {
        QubitState& qstate = this->UseQubit(q);
        for (OpId idPending : qstate.pendingZeroDurationOps)
        {
            this->AddOperationToLayer(idPending, layer);
        }

        // Update the qubit state.
        qstate.layer          = layer;
        const Time layerStart = this->metricsByLayer[(size_t)layer].startTime;
        qstate.lastUsedTime   = std::max(layerStart, qstate.lastUsedTime) + opDuration;
        qstate.pendingZeroDurationOps.clear();
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::TraceSingleQubitOp
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::TraceSingleQubitOp(OpId id, Duration opDuration, QubitIdType target)
    {
        this->seenOps.insert(id);

        QubitState& qstate    = this->UseQubit(target);
        const LayerId barrier = this->GetEffectiveFence();
        if (opDuration == 0 && (qstate.layer == INVALID || (barrier != INVALID && qstate.layer < barrier)))
        {
            qstate.pendingZeroDurationOps.push_back(id);
            return INVALID;
        }

        // Figure out the layer this operation should go into.
        LayerId layerToInsertInto = this->FindLayerToInsertOperationInto(target, opDuration);
        if (layerToInsertInto == REQUESTNEW)
        {
            layerToInsertInto = this->CreateNewLayer(opDuration);
        }

        // Add the operation and the pending zero-duration ones into the layer.
        this->AddOperationToLayer(id, layerToInsertInto);
        this->UpdateQubitState(target, layerToInsertInto, opDuration);

        return layerToInsertInto;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::TraceMultiQubitOp
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::TraceMultiQubitOp(OpId id, Duration opDuration, long nFirstGroup, QubitIdType* firstGroup,
                                       long nSecondGroup, QubitIdType* secondGroup)
    {
        assert(nFirstGroup >= 0);
        assert(nSecondGroup > 0);

        // Special-casing operations of duration zero enables potentially better reuse of qubits, when we'll start
        // optimizing for circuit width. However, tracking _the same_ pending operation across _multiple_ qubits is
        // tricky and not worth the effort, so we only do single qubit case.
        if (opDuration == 0 && nFirstGroup == 0 && nSecondGroup == 1)
        {
            return this->TraceSingleQubitOp(id, opDuration, secondGroup[0]);
        }

        this->seenOps.insert(id);

        // Figure out the layer this operation should go into.
        LayerId layerToInsertInto = this->FindLayerToInsertOperationInto(secondGroup[0], opDuration);
        for (long i = 1; i < nSecondGroup && layerToInsertInto != REQUESTNEW; i++)
        {
            layerToInsertInto =
                std::max(layerToInsertInto, this->FindLayerToInsertOperationInto(secondGroup[i], opDuration));
        }
        for (long i = 0; i < nFirstGroup && layerToInsertInto != REQUESTNEW; i++)
        {
            layerToInsertInto =
                std::max(layerToInsertInto, this->FindLayerToInsertOperationInto(firstGroup[i], opDuration));
        }
        if (layerToInsertInto == REQUESTNEW)
        {
            layerToInsertInto = this->CreateNewLayer(opDuration);
        }

        // Add the operation into the layer.
        this->AddOperationToLayer(id, layerToInsertInto);

        // Update the state of the involved qubits.
        for (long i = 0; i < nFirstGroup; i++)
        {
            this->UpdateQubitState(firstGroup[i], layerToInsertInto, opDuration);
        }
        for (long i = 0; i < nSecondGroup; i++)
        {
            this->UpdateQubitState(secondGroup[i], layerToInsertInto, opDuration);
        }

        return layerToInsertInto;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::InjectGlobalBarrier
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::InjectGlobalBarrier(OpId id, Duration duration)
    {
        LayerId layer                                 = this->CreateNewLayer(duration);
        this->metricsByLayer[(size_t)layer].barrierId = id;
        this->globalBarrier                           = layer;
        return layer;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::TraceSingleQubitMeasurement
    //------------------------------------------------------------------------------------------------------------------
    Result CTracer::TraceSingleQubitMeasurement(OpId id, Duration duration, QubitIdType target)
    {
        LayerId layerId = this->TraceSingleQubitOp(id, duration, target);
        return reinterpret_cast<Result>(layerId);
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::TraceMultiQubitMeasurement
    //------------------------------------------------------------------------------------------------------------------
    Result CTracer::TraceMultiQubitMeasurement(OpId id, Duration duration, long nTargets, QubitIdType* targets)
    {
        LayerId layerId = this->TraceMultiQubitOp(id, duration, 0, nullptr, nTargets, targets);
        return reinterpret_cast<Result>(layerId);
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::FindLatestMeasurementLayer
    //------------------------------------------------------------------------------------------------------------------
    LayerId CTracer::FindLatestMeasurementLayer(long count, Result* results)
    {
        LayerId latest = INVALID;
        for (long i = 0; i < count; i++)
        {
            const LayerId id = this->GetLayerIdOfSourceMeasurement(results[i]);
            latest           = CTracer::LaterLayerOf(latest, id);
        }
        return latest;
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::FenceScope
    //------------------------------------------------------------------------------------------------------------------
    CTracer::FenceScope::FenceScope(CTracer* trc, long count1, Result* rs1, long count2, Result* rs2) : tracer(trc)
    {
        const LayerId fence1 =
            (rs1 != nullptr && count1 > 0) ? this->tracer->FindLatestMeasurementLayer(count1, rs1) : INVALID;
        const LayerId fence2 =
            (rs2 != nullptr && count2 > 0) ? this->tracer->FindLatestMeasurementLayer(count2, rs2) : INVALID;

        this->fence = CTracer::LaterLayerOf(fence1, fence2);
        if (this->fence == INVALID)
        {
            return;
        }
        assert((size_t)(this->fence) < this->tracer->metricsByLayer.size());

        this->tracer->conditionalFences.push_back(this->fence);
        this->tracer->latestConditionalFence = CTracer::LaterLayerOf(this->tracer->latestConditionalFence, this->fence);
    }
    CTracer::FenceScope::~FenceScope()
    {
        if (this->fence == INVALID)
        {
            return;
        }

        std::vector<LayerId>& fences = this->tracer->conditionalFences;
        assert(!fences.empty());
        this->tracer->conditionalFences.pop_back();

        // Update the latest layer (we expect the stack of fences to be shallow so a linear search through it
        // should be OK).
        this->tracer->latestConditionalFence =
            fences.empty() ? INVALID : *std::max_element(fences.begin(), fences.end());
    }

    //------------------------------------------------------------------------------------------------------------------
    // CTracer::PrintLayerMetrics
    //------------------------------------------------------------------------------------------------------------------
    static std::string GetOperationName(OpId opId, const std::unordered_map<OpId, std::string>& opNames)
    {
        if (opId < 0)
        {
            return "";
        }

        auto nameIt = opNames.find(opId);
        return nameIt == opNames.end() ? std::to_string(opId) : nameIt->second;
    }
    void CTracer::PrintLayerMetrics(std::ostream& out, const std::string& separator, bool printZeroMetrics) const
    {
        // Sort the operations by id so the output is deterministic.
        std::set<OpId> seenOpsOrderedById(this->seenOps.begin(), this->seenOps.end());

        // header row
        out << "layer_id" << separator << "name";
        for (OpId opId : seenOpsOrderedById)
        {
            out << separator << GetOperationName(opId, this->opNames);
        }
        out << std::endl;

        // data rows
        const std::string zeroString = printZeroMetrics ? "0" : "";
        for (const Layer& layer : this->metricsByLayer)
        {
            out << layer.startTime;
            out << separator << GetOperationName(layer.barrierId, this->opNames);

            for (OpId opId : seenOpsOrderedById)
            {
                auto foundInLayer = layer.operations.find(opId);
                out << separator
                    << ((foundInLayer == layer.operations.end()) ? zeroString : std::to_string(foundInLayer->second));
            }
            out << std::endl;
        }
    }
} // namespace Quantum
} // namespace Microsoft
