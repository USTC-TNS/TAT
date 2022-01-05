#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2020-2021 Hao Zhang<zh970205@mail.ustc.edu.cn>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

from __future__ import annotations
import pickle
import signal
import TAT
from .sampling_lattice import SamplingLattice, DirectSampling, SweepSampling, ErgodicSampling, Observer
from .common_variable import show, showln, mpi_comm, mpi_rank, mpi_size


class SigintHandler():

    def __init__(self):
        self.sigint_recv = 0
        self.saved_handler = None

    def begin(self):

        def handler(signum, frame):
            if self.sigint_recv == 1:
                self.saved_handler(signum, frame)
            else:
                self.sigint_recv = 1

        self.saved_handler = signal.signal(signal.SIGINT, handler)

    def __call__(self):
        if self.sigint_recv:
            print(f" process {mpi_rank} receive SIGINT")
        result = mpi_comm.allreduce(self.sigint_recv)
        self.sigint_recv = 0
        return result != 0

    def end(self):
        signal.signal(signal.SIGINT, self.saved_handler)


class SeedDiffer:
    max_int = 2**31
    random_int = TAT.random.uniform_int(0, max_int - 1)

    def make_seed_diff(self):
        TAT.random.seed((self.random_int() + mpi_rank) % self.max_int)

    def make_seed_same(self):
        TAT.random.seed(mpi_comm.allreduce(self.random_int() // mpi_size))

    def __enter__(self):
        self.make_seed_diff()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.make_seed_same()

    def __init__(self):
        self.make_seed_same()


seed_differ = SeedDiffer()


def gradient_descent(state: SamplingLattice, config):

    # Sampling method
    if config.sampling_method == "sweep":
        # Use direct sampling to find sweep sampling initial configuration.
        sampling = DirectSampling(state, config.direct_sampling_cut_dimension)
        sampling()
        configuration = sampling.configuration
        sampling = SweepSampling(state)
        sampling.configuration = configuration
        sampling_total_step = config.sampling_total_step
    elif config.sampling_method == "ergodic":
        sampling = ErgodicSampling(state)
        sampling_total_step = sampling.total_step
    elif config.sampling_method == "direct":
        sampling = DirectSampling(state, config.direct_sampling_cut_dimension)
        sampling_total_step = config.sampling_total_step

    # Prepare observers
    observer = Observer(state)
    observer.add_energy()
    if config.use_gradient:
        observer.enable_gradient()
        if config.use_natural_gradient:
            observer.enable_natural_gradient()
        need_reweight_observer = config.use_line_search or config.check_difference
        if need_reweight_observer:
            reweight_observer = Observer(state)
            reweight_observer.add_energy()
            reweight_observer.enable_gradient()
        step_size = config.grad_step_size

    if config.use_gradient:
        if config.check_difference:
            grad_total_step = 1
        else:
            grad_total_step = config.grad_total_step
    else:
        grad_total_step = 1

    sigint_handler = SigintHandler()
    sigint_handler.begin()
    for grad_step in range(grad_total_step):
        if need_reweight_observer:
            configuration_pool = []
        # Sampling and observe
        with seed_differ, observer:
            for sampling_step in range(sampling_total_step):
                if sampling_step % mpi_size == mpi_rank:
                    possibility, configuration = sampling()
                    observer(possibility, configuration)
                    if need_reweight_observer:
                        configuration_pool.append((possibility, configuration.copy()))
                    show(f"sampling, total_step={sampling_total_step}, energy={observer.energy}, step={sampling_step}")
        showln(f"sampling done, total_step={sampling_total_step}, energy={observer.energy}")
        if config.use_gradient:
            # Save log
            if config.log_file and mpi_rank == 0:
                with open(config.log_file, "a") as file:
                    print(*observer.energy, file=file)

            # Get gradient
            if config.use_natural_gradient:
                show("calculating natural gradient")
                grad = observer.natural_gradient(config.conjugate_gradient_method_step, config.metric_inverse_epsilon)
                showln("calculate natural gradient done")
            else:
                grad = observer.gradient

            # Change state
            if config.check_difference:
                showln("checking difference")

                def get_energy():
                    with reweight_observer:
                        for possibility, configuration in configuration_pool:
                            configuration.refresh_all()
                            reweight_observer(possibility, configuration)
                    return reweight_observer.energy[0] * state.site_number

                original_energy = observer.energy[0] * state.site_number
                delta = 1e-8
                for l1 in range(state.L1):
                    for l2 in range(state.L2):
                        showln(l1, l2)
                        s = state[l1, l2].storage
                        g = grad[l1][l2].conjugate(positive_contract=True).storage
                        for i in range(len(s)):
                            s[i] += delta
                            now_energy = get_energy()
                            rgrad = (now_energy - original_energy) / delta
                            s[i] -= delta
                            if state.Tensor.is_complex:
                                s[i] += delta * 1j
                                now_energy = get_energy()
                                igrad = (now_energy - original_energy) / delta
                                s[i] -= delta * 1j
                                cgrad = rgrad + igrad * 1j
                            else:
                                cgrad = rgrad
                            showln(" ", cgrad, g[i])
            elif config.use_line_search:
                showln("line searching")

                saved_state = [[state[l1, l2] for l2 in range(state.L2)] for l1 in range(state.L1)]
                grad_dot_pool = {}

                param = mpi_comm.bcast(
                    (observer._lattice_dot(state._lattice, state._lattice) / observer._lattice_dot(grad, grad))**0.5,
                    root=0)

                def grad_dot(eta):
                    if eta not in grad_dot_pool:
                        for l1 in range(state.L1):
                            for l2 in range(state.L2):
                                state[l1, l2] = saved_state[l1][l2] - eta * param * grad[l1][l2].conjugate(
                                    positive_contract=True)
                        with reweight_observer:
                            for possibility, configuration in configuration_pool:
                                configuration.refresh_all()
                                reweight_observer(possibility, configuration)
                                show(f"predicting eta={eta}, energy={reweight_observer.energy}")
                        result = mpi_comm.bcast(observer._lattice_dot(grad, reweight_observer.gradient), root=0)
                        showln(f"predict eta={eta}, energy={reweight_observer.energy}, gradient dot={result}")
                        grad_dot_pool[eta] = result
                    return grad_dot_pool[eta]

                grad_dot_pool[0] = mpi_comm.bcast(observer._lattice_dot(grad, observer.gradient), root=0)
                if grad_dot(0.0) > 0:
                    begin = 0.0
                    end = step_size * 1.25

                    if grad_dot(end) > 0:
                        step_size = end
                        showln(f"step_size is chosen as {step_size}, since grad_dot(begin) > 0, grad_dot(end) > 0")
                    else:
                        while True:
                            x = (begin + end) / 2
                            if grad_dot(x) > 0:
                                begin = x
                            else:
                                end = x
                            # Step error is 0.1 now
                            if (end - begin) / end < 0.1:
                                step_size = begin
                                showln(f"step_size is chosen as {step_size}, since step size error < 0.1")
                                break
                else:
                    showln(f"step_size is chosen as {step_size}, since grad_dot(begin) < 0")
                    step_size = step_size

                real_step_size = step_size * param
                for l1 in range(state.L1):
                    for l2 in range(state.L2):
                        state[
                            l1,
                            l2] = saved_state[l1][l2] - real_step_size * grad[l1][l2].conjugate(positive_contract=True)
            else:
                for l1 in range(state.L1):
                    for l2 in range(state.L2):
                        state[l1, l2] -= step_size * grad[l1][l2].conjugate(positive_contract=True)
            showln(f"grad {grad_step}/{grad_total_step}, step_size={step_size}")

            # Bcast state and refresh sampling(refresh sampling aux and sampling config)
            for l1 in range(state.L1):
                for l2 in range(state.L2):
                    state[l1, l2] = mpi_comm.bcast(state[l1, l2], root=0)
            sampling.refresh_all()

            # Save state
            save_state_file = config.save_state_file(grad_step)
            if save_state_file and mpi_rank == 0:
                with open(save_state_file, "w") as file:
                    pickle.dump(state, file)
        if sigint_handler():
            break
    sigint_handler.end()
