/**
 * \file tensor.hpp
 *
 * Copyright (C) 2019-2021 Hao Zhang<zh970205@mail.ustc.edu.cn>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef TAT_TENSOR_HPP
#define TAT_TENSOR_HPP

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <variant>

#include "../utility/allocator.hpp"
#include "../utility/propagate_const.hpp"
#include "core.hpp"
#include "edge.hpp"
#include "name.hpp"
#include "symmetry.hpp"

namespace TAT {
   struct RemainCut {
      Size value;
      // implicit to be compatible with former interface
      RemainCut(Size v) : value(v) {}
   };
   struct RelativeCut {
      double value;
      explicit RelativeCut(double v) : value(v) {}
   };
   struct NoCut {};
   /**
    * Used to describle how to cut when doing svd to a tensor
    *
    * Is one of RemainCut, RelativeCut and NoCut
    */
   using Cut = std::variant<RemainCut, RelativeCut, NoCut>;

   /**
    * Check list of names is a valid and the rank is correct
    *
    * Only used in tensor construction
    */
   template<typename Name, typename = std::enable_if_t<is_name<Name>>>
   bool check_valid_name(const std::vector<Name>& names, const Rank& rank) {
      if (names.size() != rank) {
         detail::error("Wrong name list length which no equals to expected length");
         return false;
      }
      for (auto i = names.begin(); i != names.end(); ++i) {
         for (auto j = std::next(i); j != names.end(); ++j) {
            if (*i == *j) {
               detail::error("Duplicated names in name list");
               return false;
            }
         }
      }
      return true;
   }

   template<typename ScalarType = double, typename Symmetry = Symmetry<>, typename Name = DefaultName>
   struct TensorShape;

   /**
    * Tensor type
    *
    * tensor type contains edge name, edge shape, and tensor content.
    * every edge has a Name as its name, for nom-symmetric tensor, an edge is
    * just a number describing its dimension.
    * for symmetric tensor, an edge is a segment like structure, describing
    * each symmetry's dimension.
    * tensor content is represented as several blocks, for non-symmetric tensor,
    * there is only one block
    *
    * \tparam ScalarType scalar type of tensor content
    * \tparam Symmetry tensor's symmetry
    * \tparam Name name type to distinguish different edge
    */
   template<typename ScalarType = double, typename Symmetry = Symmetry<>, typename Name = DefaultName>
   struct Tensor {
      static_assert(is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>);

      using self_t = Tensor<ScalarType, Symmetry, Name>;
      // common used type alias
      using scalar_t = ScalarType;
      using symmetry_t = Symmetry;
      using name_t = Name;
      using edge_t = Edge<Symmetry>;
      using core_t = Core<ScalarType, Symmetry>;

      // tensor data
      // names
      /**
       * name of tensor's edge
       * \see Name
       */
      std::vector<Name> names;

      Rank get_rank() const {
         return names.size();
      }

      auto find_rank_from_name(const Name& name) const {
         return std::find(names.begin(), names.end(), name);
      }

      Rank get_rank_from_name(const Name& name) const {
         auto where = find_rank_from_name(name);
         if (where == names.end()) {
            detail::error("No such name in name list");
         }
         return std::distance(names.begin(), where);
      }

      // core
      /**
       * tensor data except name, including edge and block
       * \see Core
       * \note bacause edge rename is very common operation, to avoid copy data, put the remaining data into shared pointer
       */
      detail::propagate_const_shared_ptr<core_t> core;

      // shape
      /**
       * Get tensor shape to print, used when you don't want to know value of the tensor
       */
      TensorShape<ScalarType, Symmetry, Name> shape() {
         return {this};
      }

      // constructors
      // There are many method to construct edge, so it is not proper to use initializer list
      /**
       * Initialize tensor with tensor edge name and tensor edge shape, blocks will be generated by edges
       *
       * \param names_init edge name
       * \param edges_init edge shape
       * \see Core
       */
      Tensor(std::vector<Name> names_init, std::vector<Edge<Symmetry>> edges_init) :
            names(std::move(names_init)),
            core(std::make_shared<core_t>(std::move(edges_init))) {
         if constexpr (debug_mode) {
            check_valid_name(names, core->edges.size());
         }
      }

      Tensor() : Tensor(1){};
      Tensor(const Tensor& other) = default;
      Tensor(Tensor&& other) noexcept = default;
      Tensor& operator=(const Tensor& other) = default;
      Tensor& operator=(Tensor&& other) noexcept = default;
      ~Tensor() = default;

      /**
       * create a rank-0 tensor
       * \param number the only element of this tensor
       */
      explicit Tensor(ScalarType number) : Tensor({}, {}) {
         storage().front() = number;
      }

      [[nodiscard]] bool scalar_like() const {
         return storage().size() == 1;
      }

      /**
       * Get the only element from a tensor which contains only one element
       */
      explicit operator ScalarType() const {
         return const_at();
      }

      /// \private
      [[nodiscard]] static auto
      get_edge_from_edge_symmetry_and_arrow(const std::vector<Symmetry>& edge_symmetry, const std::vector<Arrow>& edge_arrow, Rank rank) {
         // used in one
         if constexpr (Symmetry::length == 0) {
            return std::vector<Edge<Symmetry>>(rank, {1});
         } else {
            auto result = std::vector<Edge<Symmetry>>();
            result.reserve(rank);
            for (auto [symmetry, arrow] = std::tuple{edge_symmetry.begin(), edge_arrow.begin()}; symmetry < edge_symmetry.end();
                 ++symmetry, ++arrow) {
               if constexpr (Symmetry::is_fermi_symmetry) {
                  result.push_back({{{*symmetry, 1}}, *arrow});
               } else {
                  result.push_back({{{*symmetry, 1}}});
               }
            }
            return result;
         }
      }
      /**
       * Create a high rank tensor but which only contains one element
       *
       * \note Tensor::one(a, {}, {}, {}) is equivilent to Tensor(a)
       * \param number the only element
       * \param names_init edge name
       * \param edge_symmetry the symmetry for every edge, if valid
       * \param edge_arrow the fermi arrow for every edge, if valid
       */
      [[nodiscard]] static Tensor<ScalarType, Symmetry, Name>
      one(ScalarType number,
          std::vector<Name> names_init,
          const std::vector<Symmetry>& edge_symmetry = {},
          const std::vector<Arrow>& edge_arrow = {}) {
         const auto rank = names_init.size();
         auto result = Tensor(std::move(names_init), get_edge_from_edge_symmetry_and_arrow(edge_symmetry, edge_arrow, rank));
         result.storage().front() = number;
         return result;
      }

      // elementwise operators
      /**
       * Do the same operator to the every value element of the tensor, inplacely
       * \param function The operator
       */
      template<typename Function>
      Tensor<ScalarType, Symmetry, Name>& transform(Function&& function) & {
         acquare_data_ownership("Set tensor shared, copy happened here");
         std::transform(storage().begin(), storage().end(), storage().begin(), function);
         return *this;
      }
      template<typename Function>
      Tensor<ScalarType, Symmetry, Name>&& transform(Function&& function) && {
         return std::move(transform(function));
      }

      /**
       * Generate a tensor with the same shape
       * \tparam NewScalarType basic scalar type of the result tensor
       * \return The value of tensor is not initialized
       */
      template<typename NewScalarType = ScalarType>
      [[nodiscard]] Tensor<NewScalarType, Symmetry, Name> same_shape() const {
         return Tensor<NewScalarType, Symmetry, Name>(names, core->edges);
      }

      /**
       * Do the same operator to the every value element of the tensor, outplacely
       * \param function The operator
       * \return The result tensor
       * \see same_shape
       */
      template<typename ForceScalarType = void, typename Function>
      [[nodiscard]] auto map(Function&& function) const {
         using DefaultNewScalarType = std::result_of_t<Function(ScalarType)>;
         using NewScalarType = std::conditional_t<std::is_same_v<void, ForceScalarType>, DefaultNewScalarType, ForceScalarType>;
         auto result = same_shape<NewScalarType>();
         std::transform(storage().begin(), storage().end(), result.storage().begin(), function);
         return result;
      }

      /**
       * Tensor deep copy, default copy will share the common data, i.e. the same core
       * \see map
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> copy() const {
         return map([](auto x) {
            return x;
         });
      }

      /**
       * Set value of tensor by a generator elementwisely
       * \param generator Generator accept non argument, and return scalartype
       */
      template<typename Generator>
      Tensor<ScalarType, Symmetry, Name>& set(Generator&& generator) & {
         acquare_data_ownership("Set tensor shared, copy happened here");
         std::generate(storage().begin(), storage().end(), generator);
         return *this;
      }
      template<typename Generator>
      Tensor<ScalarType, Symmetry, Name>&& set(Generator&& generator) && {
         return std::move(set(generator));
      }

      /**
       * Set all the value of the tensor to zero
       * \see set
       */
      Tensor<ScalarType, Symmetry, Name>& zero() & {
         return set([]() {
            return 0;
         });
      }
      Tensor<ScalarType, Symmetry, Name>&& zero() && {
         return std::move(zero());
      }

      /**
       * Set the value of tensor as natural number, used for test
       * \see set
       */
      Tensor<ScalarType, Symmetry, Name>& range(ScalarType first = 0, ScalarType step = 1) & {
         return set([&first, step]() {
            auto result = first;
            first += step;
            return result;
         });
      }
      Tensor<ScalarType, Symmetry, Name>&& range(ScalarType first = 0, ScalarType step = 1) && {
         return std::move(range(first, step));
      }

      /**
       * Acquare tensor data's ownership, it will copy the core if the core is shared
       * \param message warning message if core is copied
       */
      void acquare_data_ownership(const char* message) {
         if (core.use_count() != 1) {
            core = std::make_shared<Core<ScalarType, Symmetry>>(*core);
            detail::what_if_copy_shared(message);
         }
      }

      /**
       * Change the basic scalar type of the tensor
       */
      template<typename OtherScalarType, typename = std::enable_if_t<is_scalar<OtherScalarType>>>
      [[nodiscard]] Tensor<OtherScalarType, Symmetry, Name> to() const {
         if constexpr (std::is_same_v<ScalarType, OtherScalarType>) {
            return *this;
         } else {
            return map([](ScalarType input) -> OtherScalarType {
               if constexpr (is_complex<ScalarType> && is_real<OtherScalarType>) {
                  return OtherScalarType(input.real());
               } else {
                  return OtherScalarType(input);
               }
            });
         }
      }

      /**
       * Get the norm of the tensor
       * \note Treat the tensor as vector, not the matrix norm or other things
       * \tparam p Get the p-norm of the tensor, if p=-1, that is max absolute value norm, namely inf-norm
       */
      template<int p = 2>
      [[nodiscard]] real_scalar<ScalarType> norm() const {
         real_scalar<ScalarType> result = 0;
         if constexpr (p == -1) {
            // max abs
            for (const auto& number : storage()) {
               if (auto absolute_value = std::abs(number); absolute_value > result) {
                  result = absolute_value;
               }
            }
         } else if constexpr (p == 0) {
            result += real_scalar<ScalarType>(storage().size());
         } else {
            for (const auto& number : storage()) {
               if constexpr (p == 1) {
                  result += std::abs(number);
               } else if constexpr (p == 2) {
                  result += std::norm(number);
               } else {
                  if constexpr (p % 2 == 0 && is_real<ScalarType>) {
                     result += std::pow(number, p);
                  } else {
                     result += std::pow(std::abs(number), p);
                  }
               }
            }
            result = std::pow(result, 1. / p);
         }
         return result;
      }

      // get element or other things
      [[nodiscard]] const ScalarType& at(const std::map<Name, std::pair<Symmetry, Size>>& position) const& {
         return const_at(position);
      }

      [[nodiscard]] const ScalarType& at(const std::map<Name, Size>& position) const& {
         return const_at(position);
      }

      [[nodiscard]] const ScalarType& at() const& {
         return const_at();
      }

      [[nodiscard]] ScalarType& at(const std::map<Name, std::pair<Symmetry, Size>>& position) & {
         acquare_data_ownership("Get reference which may change of shared tensor, copy happened here, use const_at to get const reference");
         return const_cast<ScalarType&>(const_cast<const self_t*>(this)->const_at(position));
      }

      [[nodiscard]] ScalarType& at(const std::map<Name, Size>& position) & {
         acquare_data_ownership("Get reference which may change of shared tensor, copy happened here, use const_at to get const reference");
         return const_cast<ScalarType&>(const_cast<const self_t*>(this)->const_at(position));
      }

      [[nodiscard]] ScalarType& at() & {
         acquare_data_ownership("Get reference which may change of shared tensor, copy happened here, use const_at to get const reference");
         return const_cast<ScalarType&>(const_cast<const self_t*>(this)->const_at());
      }

      [[nodiscard]] const ScalarType& const_at(const std::map<Name, std::pair<Symmetry, Size>>& position) const& {
         return get_item(position);
      }

      [[nodiscard]] const ScalarType& const_at(const std::map<Name, Size>& position) const& {
         return get_item(position);
      }

      [[nodiscard]] const ScalarType& const_at() const& {
         if (!scalar_like()) {
            detail::error("Try to get the only element of t he tensor which contains more than one element");
         }
         return storage().front();
      }

      /// \private
      template<typename IndexOrPoint>
      [[nodiscard]] const ScalarType& get_item(const std::map<Name, IndexOrPoint>& position) const&;

      Tensor<ScalarType, NoSymmetry, Name> clear_symmetry() const;

      const auto& storage() const& {
         return core->storage;
      }
      auto& storage() & {
         return core->storage;
      }

      const Edge<Symmetry>& edges(Rank r) const& {
         return core->edges[r];
      }
      const Edge<Symmetry>& edges(const Name& name) const& {
         return edges(get_rank_from_name(name));
      }
      Edge<Symmetry>& edges(Rank r) & {
         return const_cast<Edge<Symmetry>&>(const_cast<const self_t*>(this)->edges(r));
      }
      Edge<Symmetry>& edges(const Name& name) & {
         return const_cast<Edge<Symmetry>&>(const_cast<const self_t*>(this)->edges(name));
      }

      /// \private
      /**
       * If given a Key, return itself, else return a.first;
       */
      template<typename Key, typename A>
      static const auto& get_key(const A& a) {
         if constexpr (std::is_same_v<remove_cvref_t<Key>, remove_cvref_t<A>>) {
            return a;
         } else {
            return std::get<0>(a);
         }
      }

      template<typename SymmetryList = std::vector<Symmetry>>
      auto find_block(const SymmetryList& symmetry_list) const {
         using Key = SymmetryList;
         const auto& v = core->blocks;
         const auto& key = symmetry_list;
         auto result = std::lower_bound(v.begin(), v.end(), key, [](const auto& a, const auto& b) {
            return std::lexicographical_compare(get_key<Key>(a).begin(), get_key<Key>(a).end(), get_key<Key>(b).begin(), get_key<Key>(b).end());
         });
         if (result == v.end()) {
            // result may be un dereferencable
            return v.end();
         } else if (std::equal(result->first.begin(), result->first.end(), key.begin(), key.end())) {
            return result;
         } else {
            return v.end();
         }
      }

      template<typename SymmetryList = std::vector<Symmetry>>
      const typename core_t::content_vector_t& blocks(const SymmetryList& symmetry_list) const& {
         auto found = find_block(symmetry_list);
         if (found == core->blocks.end()) {
            detail::error("No such symmetry block in the tensor");
         }
         return found->second;
      }
      const typename core_t::content_vector_t& blocks(const std::map<Name, Symmetry>& symmetry_map) const& {
         std::vector<Symmetry> symmetry_list;
         symmetry_list.reserve(get_rank());
         for (const auto& name : names) {
            symmetry_list.push_back(symmetry_map.at(name));
         }
         return blocks(symmetry_list);
      }
      template<typename SymmetryList = std::vector<Symmetry>>
      typename core_t::content_vector_t& blocks(const SymmetryList& symmetry_list) & {
         return const_cast<typename core_t::content_vector_t&>(const_cast<const self_t*>(this)->blocks(symmetry_list));
      }
      typename core_t::content_vector_t& blocks(const std::map<Name, Symmetry>& symmetry_map) & {
         return const_cast<typename core_t::content_vector_t&>(const_cast<const self_t*>(this)->blocks(symmetry_map));
      }

      // Operators
      /**
       * 对张量的边进行操作的中枢函数, 对边依次做重命名, 分裂, 费米箭头取反, 合并, 转置的操作,
       * \param rename_map 重命名边的名称的映射表
       * \param split_map 分裂一些边的数据, 需要包含分裂后边的形状, 不然分裂不唯一
       * \param reversed_name 将要取反费米箭头的边的名称列表
       * \param merge_map 合并一些边的名称列表
       * \param new_names 最后进行的转置操作后的边的名称顺序列表
       * \param apply_parity 控制费米对称性中费米性质产生的符号是否应用在结果张量上的默认行为
       * \param parity_exclude_name 是否产生符号这个问题上行为与默认行为相反的操作的边的名称, 四部分分别是split, reverse, reverse_before_merge, merge
       * \return 进行了一系列操作后的结果张量
       * \note 反转不满足和合并操作的条件时, 将在合并前再次反转需要反转的边, 方向对齐第一个有方向的边
       * \note 因为费米箭头在反转和合并分裂时会产生半个符号, 所以需要扔给一方张量, 另一方张量不变号
       * \note 但是转置部分时产生一个符号的, 所以这一部分无视apply_parity
       * \note 本函数对转置外不标准的腿的输入是脆弱的
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> edge_operator(
            const std::map<Name, std::vector<std::pair<Name, edge_segment_t<Symmetry>>>>& split_map,
            const std::set<Name>& reversed_name,
            const std::map<Name, std::vector<Name>>& merge_map, // 这个可以merge后再对edge进行reorder
            std::vector<Name> new_names,
            const bool apply_parity = false,
            const std::set<Name>& parity_exclude_name_split = {},
            const std::set<Name>& parity_exclude_name_reversed_before_transpose = {},
            const std::set<Name>& parity_exclude_name_reversed_after_transpose = {},
            const std::set<Name>& parity_exclude_name_merge = {}) const {
         auto pmr_guard = scope_resource(default_buffer_size);
         return edge_operator_implement(
               split_map,
               reversed_name,
               merge_map,
               std::move(new_names),
               apply_parity,
               parity_exclude_name_split,
               parity_exclude_name_reversed_before_transpose,
               parity_exclude_name_reversed_after_transpose,
               parity_exclude_name_merge,
               empty_list<std::pair<Name, empty_list<std::pair<Symmetry, Size>>>>());
         // last argument only used in svd, Name -> Symmetry -> Size
      }

      /// \private
      template<typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
      [[nodiscard]] auto edge_operator_implement(
            const A& split_map,
            const B& reversed_name,
            const C& merge_map,
            std::vector<Name> new_names,
            const bool apply_parity,
            const D& parity_exclude_name_split,
            const E& parity_exclude_name_reversed_before_transpose,
            const F& parity_exclude_name_reversed_after_transpose,
            const G& parity_exclude_name_merge,
            const H& edge_and_symmetries_to_cut_before_all = {}) const;
      /**
       * 对张量边的名称进行重命名
       * \param dictionary 重命名方案的映射表
       * \return 仅仅改变了边的名称的张量, 与原张量共享Core
       * \note 虽然功能蕴含于edge_operator中, 但是edge_rename操作很常用, 所以并没有调用会稍微慢的edge_operator, 而是实现一个小功能的edge_rename
       */
      template<typename ResultName = Name, typename = std::enable_if_t<is_name<ResultName>>>
      [[nodiscard]] auto edge_rename(const std::map<Name, ResultName>& dictionary) const;

      /**
       * 对张量进行转置
       * \param target_names 转置后的目标边的名称顺序
       * \return 转置后的结果张量
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> transpose(std::vector<Name> target_names) const {
         auto pmr_guard = scope_resource(default_buffer_size);
         return edge_operator_implement(
               empty_list<std::pair<Name, empty_list<std::pair<Name, edge_segment_t<Symmetry>>>>>(),
               empty_list<Name>(),
               empty_list<std::pair<Name, empty_list<Name>>>(),
               std::move(target_names),
               false,
               empty_list<Name>(),
               empty_list<Name>(),
               empty_list<Name>(),
               empty_list<Name>(),
               empty_list<std::pair<Name, empty_list<std::pair<Symmetry, Size>>>>());
      }

      /**
       * 将费米张量的一些边进行反转
       * \param reversed_name 反转的边的集合
       * \param apply_parity 是否应用反转产生的符号
       * \param parity_exclude_name 与apply_parity行为相反的边名集合
       * \return 反转后的结果张量
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name>
      reverse_edge(const std::set<Name>& reversed_name, bool apply_parity = false, const std::set<Name>& parity_exclude_name = {}) const {
         auto pmr_guard = scope_resource(default_buffer_size);
         return edge_operator_implement(
               empty_list<std::pair<Name, empty_list<std::pair<Name, edge_segment_t<Symmetry>>>>>(),
               reversed_name,
               empty_list<std::pair<Name, empty_list<Name>>>(),
               names,
               apply_parity,
               empty_list<Name>(),
               parity_exclude_name,
               empty_list<Name>(),
               empty_list<Name>(),
               empty_list<std::pair<Name, empty_list<std::pair<Symmetry, Size>>>>());
      }

      /**
       * 合并张量的一些边
       * \param merge 合并的边的名称的映射表
       * \param apply_parity 是否应用合并边产生的符号
       * \param parity_exclude_name_merge merge过程中与apply_parity不符的例外
       * \param parity_exclude_name_reverse merge前不得不做的reverse过程中与apply_parity不符的例外
       * \return 合并边后的结果张量
       * \note 合并前转置的策略是将一组合并的边按照合并时的顺序移动到这组合并边中最后的一个边前, 其他边位置不变
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> merge_edge(
            const std::map<Name, std::vector<Name>>& merge,
            bool apply_parity = false,
            const std::set<Name>&& parity_exclude_name_merge = {},
            const std::set<Name>& parity_exclude_name_reverse = {}) const;
      /**
       * 分裂张量的一些边
       * \param split 分裂的边的名称的映射表
       * \param apply_parity 是否应用分裂边产生的符号
       * \param parity_exclude_name_split split过程中与apply_parity不符的例外
       * \return 分裂边后的结果张量
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> split_edge(
            const std::map<Name, std::vector<std::pair<Name, edge_segment_t<Symmetry>>>>& split,
            bool apply_parity = false,
            const std::set<Name>& parity_exclude_name_split = {}) const;

      // Contract
      // 可以考虑不转置成矩阵直接乘积的可能, 但这个最多优化N^2的常数次, 只需要转置不调用多次就不会产生太大的问题
      /// \private
      [[nodiscard]] static Tensor<ScalarType, Symmetry, Name> contract(
            const Tensor<ScalarType, Symmetry, Name>& tensor_1,
            const Tensor<ScalarType, Symmetry, Name>& tensor_2,
            std::set<std::pair<Name, Name>> contract_names);

      /**
       * 两个张量的缩并运算
       * \param tensor_1 参与缩并的第一个张量
       * \param tensor_2 参与缩并的第二个张量
       * \param contract_names 两个张量将要缩并掉的边的名称
       * \return 缩并后的张量
       */
      template<typename ScalarType1, typename ScalarType2, typename = std::enable_if_t<is_scalar<ScalarType1> && is_scalar<ScalarType2>>>
      [[nodiscard]] static auto contract(
            const Tensor<ScalarType1, Symmetry, Name>& tensor_1,
            const Tensor<ScalarType2, Symmetry, Name>& tensor_2,
            std::set<std::pair<Name, Name>> contract_names) {
         using ResultScalarType = std::common_type_t<ScalarType1, ScalarType2>;
         using ResultTensor = Tensor<ResultScalarType, Symmetry, Name>;
         if constexpr (std::is_same_v<ResultScalarType, ScalarType1>) {
            if constexpr (std::is_same_v<ResultScalarType, ScalarType2>) {
               return ResultTensor::contract(tensor_1, tensor_2, std::move(contract_names));
            } else {
               return ResultTensor::contract(tensor_1, tensor_2.template to<ResultScalarType>(), std::move(contract_names));
            }
         } else {
            if constexpr (std::is_same_v<ResultScalarType, ScalarType2>) {
               return ResultTensor::contract(tensor_1.template to<ResultScalarType>(), tensor_2, std::move(contract_names));
            } else {
               return ResultTensor::contract(
                     tensor_1.template to<ResultScalarType>(),
                     tensor_2.template to<ResultScalarType>(),
                     std::move(contract_names));
            }
         }
      }

      template<typename OtherScalarType, typename = std::enable_if_t<is_scalar<OtherScalarType>>>
      [[nodiscard]] auto
      contract(const Tensor<OtherScalarType, Symmetry, Name>& tensor_2, const std::set<std::pair<Name, Name>>& contract_names) const {
         return contract(*this, tensor_2, std::move(contract_names));
      }

      /**
       * 生成相同形状的单位张量
       * \param pairs 看作矩阵时边的配对方案
       */
      Tensor<ScalarType, Symmetry, Name>& identity(const std::set<std::pair<Name, Name>>& pairs) &;

      Tensor<ScalarType, Symmetry, Name>&& identity(const std::set<std::pair<Name, Name>>& pairs) && {
         return std::move(identity(pairs));
      }

      /**
       * 看作矩阵后求出矩阵指数
       * \param pairs 边的配对方案
       * \param step 迭代步数
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> exponential(const std::set<std::pair<Name, Name>>& pairs, int step = 2) const;

      /**
       * 生成张量的共轭张量
       * \note 如果为对称性张量, 量子数取反, 如果为费米张量, 箭头取反, 如果为复张量, 元素取共轭
       */
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> conjugate() const;

      [[nodiscard]] Tensor<ScalarType, Symmetry, Name> trace(const std::set<std::pair<Name, Name>>& trace_names) const;

      /**
       * 张量svd的结果类型
       * \note S的的对称性是有方向的, 用来标注如何对齐, 向U对齐
       */
      struct svd_result {
         Tensor<ScalarType, Symmetry, Name> U;
         Tensor<ScalarType, Symmetry, Name> S;
         Tensor<ScalarType, Symmetry, Name> V;
      };

      /**
       * 张量qr的结果类型
       */
      struct qr_result {
         Tensor<ScalarType, Symmetry, Name> Q;
         Tensor<ScalarType, Symmetry, Name> R;
      };

      [[nodiscard, deprecated("Explicit singular tensor edge name required in future")]] svd_result
      svd(const std::set<Name>& free_name_set_u, const Name& common_name_u, const Name& common_name_v) const {
         return svd(free_name_set_u, common_name_u, common_name_v, InternalName<Name>::SVD_U, InternalName<Name>::SVD_V, NoCut());
      }
      [[nodiscard, deprecated("Explicit singular tensor edge name required in future")]] svd_result
      svd(const std::set<Name>& free_name_set_u, const Name& common_name_u, const Name& common_name_v, Cut cut) const {
         return svd(free_name_set_u, common_name_u, common_name_v, InternalName<Name>::SVD_U, InternalName<Name>::SVD_V, cut);
      }
      [[nodiscard, deprecated("Put cut to the last argument")]] svd_result
      svd(const std::set<Name>& free_name_set_u,
          const Name& common_name_u,
          const Name& common_name_v,
          Cut cut,
          const Name& singular_name_u,
          const Name& singular_name_v) const {
         return svd(free_name_set_u, common_name_u, common_name_v, singular_name_u, singular_name_v, cut);
      }

      /**
       * 对张量进行svd分解
       * \param free_name_set_u svd分解中u的边的名称集合
       * \param common_name_u 分解后u新产生的边的名称
       * \param common_name_v 分解后v新产生的边的名称
       * \param cut 需要截断的维度数目
       * \return svd的结果
       * \see svd_result
       * \note 对于对称性张量, S需要有对称性, S对称性与V的公共边配对, 与U的公共边相同
       */
      [[nodiscard]] svd_result
      svd(const std::set<Name>& free_name_set_u,
          const Name& common_name_u,
          const Name& common_name_v,
          const Name& singular_name_u,
          const Name& singular_name_v,
          Cut cut = NoCut()) const;

      /**
       * 对张量进行qr分解
       * \param free_name_direction free_name_set取的方向, 为'Q'或'R'
       * \param free_name_set qr分解中某一侧的边的名称集合
       * \param common_name_q 分解后q新产生的边的名称
       * \param common_name_r 分解后r新产生的边的名称
       * \return qr的结果
       * \see qr_result
       */
      [[nodiscard]] qr_result
      qr(char free_name_direction, const std::set<Name>& free_name_set, const Name& common_name_q, const Name& common_name_r) const;

      using EdgePointShrink = std::conditional_t<Symmetry::length == 0, Size, std::tuple<Symmetry, Size>>;
      using EdgePointExpand = std::conditional_t<
            Symmetry::length == 0,
            std::tuple<Size, Size>,
            std::conditional_t<Symmetry::is_fermi_symmetry, std::tuple<Arrow, Symmetry, Size, Size>, std::tuple<Symmetry, Size, Size>>>;
      // index, dim

      [[nodiscard]] Tensor<ScalarType, Symmetry, Name>
      expand(const std::map<Name, EdgePointExpand>& configure, const Name& old_name = InternalName<Name>::No_Old_Name) const;
      [[nodiscard]] Tensor<ScalarType, Symmetry, Name>
      shrink(const std::map<Name, EdgePointShrink>& configure, const Name& new_name = InternalName<Name>::No_New_Name, Arrow arrow = false) const;

      /// \private
      const Tensor<ScalarType, Symmetry, Name>& meta_put(std::ostream&) const;
      /// \private
      const Tensor<ScalarType, Symmetry, Name>& data_put(std::ostream&) const;
      /// \private
      Tensor<ScalarType, Symmetry, Name>& meta_get(std::istream&);
      /// \private
      Tensor<ScalarType, Symmetry, Name>& data_get(std::istream&);

      [[nodiscard]] std::string show() const;
      [[nodiscard]] std::string dump() const;
      Tensor<ScalarType, Symmetry, Name>& load(const std::string&) &;
      Tensor<ScalarType, Symmetry, Name>&& load(const std::string& string) && {
         return std::move(load(string));
      };
   };

   namespace detail {
      template<typename T>
      struct is_tensor_helper : std::false_type {};
      template<typename A, typename B, typename C>
      struct is_tensor_helper<Tensor<A, B, C>> : std::true_type {};
   } // namespace detail
   template<typename T>
   constexpr bool is_tensor = detail::is_tensor_helper<T>::value;

   template<typename ScalarType1, typename ScalarType2, typename Symmetry, typename Name>
   [[nodiscard]] auto contract(
         const Tensor<ScalarType1, Symmetry, Name>& tensor_1,
         const Tensor<ScalarType2, Symmetry, Name>& tensor_2,
         std::set<std::pair<Name, Name>> contract_names) {
      return tensor_1.contract(tensor_2, std::move(contract_names));
   }

   template<typename ScalarType, typename Symmetry, typename Name>
   struct TensorShape {
      static_assert(is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>);

      Tensor<ScalarType, Symmetry, Name>* owner;
   };

   // TODO: middle 用edge operator表示一个待计算的张量, 在contract中用到
   // 因为contract的操作是这样的
   // merge gemm split
   // 上一次split可以和下一次的merge合并
   // 比较重要， 可以大幅减少对称性张量的分块
   /*
   template<is_scalar ScalarType, is_symmetry Symmetry, is_name Name>
   struct QuasiTensor {
      Tensor<ScalarType, Symmetry, Name> tensor;
      std::map<Name, std::vector<std::tuple<Name, edge_segment_t<Symmetry>>>> split_map;
      std::set<Name> reversed_set;
      std::vector<Name> res_name;

      QuasiTensor

      operator Tensor<ScalarType, Symmetry, Name>() && {
         return tensor.edge_operator({}, split_map, reversed_set, {}, std::move(res_name));
      }
      operator Tensor<ScalarType, Symmetry, Name>() const& {
         return tensor.edge_operator({}, split_map, reversed_set, {}, res_name);
      }

      Tensor<ScalarType, Symmetry, Name> merge_again(
            const std::set<Name>& merge_reversed_set,
            const std::map<Name, std::vector<Name>>& merge_map,
            std::vector<Name>&& merge_res_name,
            std::set<Name>& split_parity_mark,
            std::set<Name>& merge_parity_mark) {
         auto total_reversed_set = reversed_set; // merge_reversed_set
         return tensor.edge_operator(
               {},
               split_map,
               total_reversed_set,
               merge_map,
               merge_res_name,
               false,
               {{{}, split_parity_mark, {}, merge_parity_mark}});
      }
      QuasiTensor<ScalarType, Symmetry, Name>
   };
   */

   // TODO: lazy framework
   // 看一下idris是如何做的
   // 需要考虑深搜不可行的问题
   // 支持inplace操作

} // namespace TAT
#endif
