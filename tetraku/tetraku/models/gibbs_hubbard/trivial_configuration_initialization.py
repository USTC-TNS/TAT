#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2022-2023 Chao Wang<1023649157@qq.com>
# Copyright (C) 2023 Hao Zhang<zh970205@mail.ustc.edu.cn>
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

import TAT
import tetragono as tet


def initial_configuration(configuration):
    # This function initialize the configuration as anti-ferromagnetic phase, and it is one of the major terms in infinite temperature.
    state = configuration.owner
    for l1, l2 in state.sites():
        if (l1 + l2) % 2 == 0:
            configuration[l1, l2, 0] = configuration[l1, l2, 1] = (True, 0)
        else:
            configuration[l1, l2, 0] = configuration[l1, l2, 1] = (True, 1)
    return configuration
