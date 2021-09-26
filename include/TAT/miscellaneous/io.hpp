/**
 * \file io.hpp
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
#ifndef TAT_IO_HPP
#define TAT_IO_HPP

#include <iostream>
#include <limits>
#include <type_traits>

#include "../structure/tensor.hpp"

namespace TAT {
   // complex text io, complex bin io can be done directly
   namespace detail {
      template<typename ScalarType>
      std::ostream& print_complex(std::ostream& out, const std::complex<ScalarType>& value) {
         if (value.real() != 0) {
            out << value.real();
            if (value.imag() != 0) {
               if (value.imag() > 0) {
                  out << '+';
               }
               out << value.imag();
               out << 'i';
            }
         } else {
            if (value.imag() == 0) {
               out << '0';
            } else {
               out << value.imag();
               out << 'i';
            }
         }
         return out;
      }

      template<typename ScalarType>
      std::ostream&& print_complex(std::ostream&& out, const std::complex<ScalarType>& value) {
         print_complex(out, value);
         return std::move(out);
      }

      inline void ignore_until(std::istream& in, char end) {
         in.ignore(std::numeric_limits<std::streamsize>::max(), end);
      }

      template<typename ScalarType>
      std::istream& scan_complex(std::istream& in, std::complex<ScalarType>& value) {
         ScalarType part;
         in >> part;
         char maybe_i = in.peek();
         if (maybe_i == 'i') {
            in.get();
            // no real part
            value = std::complex<ScalarType>{0, part};
         } else {
            // have real part
            if (maybe_i == '+' || maybe_i == '-') {
               // have imag part
               ScalarType another_part;
               in >> another_part;
               value = std::complex<ScalarType>{part, another_part};
               if (in.get() != 'i') {
                  in.setstate(std::ios::failbit);
               }
            } else {
               // no imag part
               value = std::complex<ScalarType>{part, 0};
            }
         }
         return in;
      }

      template<typename ScalarType>
      std::istream&& scan_complex(std::istream&& in, std::complex<ScalarType>& value) {
         detail::scan_complex(in, value);
         return std::move(in);
      }
   } // namespace detail

   // trivial type bin io

   template<typename T, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
   std::ostream& operator<(std::ostream& out, const T& data) {
      out.write(reinterpret_cast<const char*>(&data), sizeof(T));
      return out;
   }
   template<typename T, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
   std::istream& operator>(std::istream& in, T& data) {
      in.read(reinterpret_cast<char*>(&data), sizeof(T));
      return in;
   }

   // name type io

   inline std::ostream& operator<<(std::ostream& out, const FastName& name) {
      return out << static_cast<const std::string&>(name);
   }

   namespace detail {
      inline bool valid_name_character(char c) {
         if (!std::isprint(c)) {
            return false;
         }
         if (c == ' ') {
            return false;
         }
         if (c == ',') {
            return false;
         }
         if (c == '[') {
            return false;
         }
         if (c == ']') {
            return false;
         }
         return true;
      }
   } // namespace detail

   // inline std::istream& operator>>(std::istream& in, std::string& name) {
   inline std::istream& scan_string_for_name(std::istream& in, std::string& name) {
      char buffer[256]; // max name length = 256
      Size length = 0;
      while (detail::valid_name_character(in.peek())) {
         buffer[length++] = in.get();
      }
      buffer[length] = '\x00';
      name = (const char*)buffer;
      return in;
   }

   inline std::istream& scan_fastname_for_name(std::istream& in, FastName& name) {
      std::string string;
      scan_string_for_name(in, string);
      name = FastName(string);
      return in;
   }

   inline std::ostream& operator<(std::ostream& out, const std::string& string) {
      Size count = string.size();
      out < count;
      out.write(string.data(), sizeof(char) * count);
      return out;
   }
   inline std::istream& operator>(std::istream& in, std::string& string) {
      Size count;
      in > count;
      string.resize(count);
      in.read(string.data(), sizeof(char) * count);
      return in;
   }

   template<>
   struct NameTraits<FastName> {
      static constexpr name_out_operator_t<FastName> write = operator<; // it is trivial type
      static constexpr name_in_operator_t<FastName> read = operator>;   // it is trivial type
      static constexpr name_out_operator_t<FastName> print = operator<<;
      static constexpr name_in_operator_t<FastName> scan = scan_fastname_for_name;
   };
   template<>
   struct NameTraits<std::string> {
      static constexpr name_out_operator_t<std::string> write = operator<;
      static constexpr name_in_operator_t<std::string> read = operator>;
      static constexpr name_out_operator_t<std::string> print = std::operator<<;
      static constexpr name_in_operator_t<std::string> scan = scan_string_for_name;
   };

   template<typename T>
   struct is_symmetry_vector : std::false_type {};
   template<typename T>
   struct is_symmetry_vector<std::vector<T>> : std::bool_constant<is_symmetry<T>> {};
   template<typename T>
   constexpr bool is_symmetry_vector_v = is_symmetry_vector<T>::value;

   // map and vector io, bin and text, map text is not needed

   template<typename T, typename A>
   std::ostream& operator<(std::ostream& out, const std::vector<T, A>& list) {
      Size count = list.size();
      out < count;
      if constexpr (std::is_trivially_destructible_v<T>) {
         out.write(reinterpret_cast<const char*>(list.data()), sizeof(T) * count);
      } else {
         for (const auto& i : list) {
            if constexpr (is_name<T>) {
               NameTraits<T>::write(out, i);
            } else {
               out < i;
            }
         }
      }
      return out;
   }
   template<typename T, typename A>
   std::istream& operator>(std::istream& in, std::vector<T, A>& list) {
      list.clear();
      Size count;
      in > count;
      if constexpr (std::is_trivially_destructible_v<T>) {
         list.resize(count);
         in.read(reinterpret_cast<char*>(list.data()), sizeof(T) * count);
      } else {
         for (Size i = 0; i < count; i++) {
            auto& item = list.emplace_back();
            if constexpr (is_name<T>) {
               NameTraits<T>::read(in, item);
            } else {
               in > item;
            }
         }
      }
      return in;
   }

   template<typename Key, typename Value, typename = std::enable_if_t<is_symmetry<Key> || is_symmetry_vector_v<Key>>>
   std::ostream& operator<(std::ostream& out, const std::map<Key, Value>& map) {
      Size size = map.size();
      out < size;
      for (const auto& [key, value] : map) {
         out < key < value;
      }
      return out;
   }

   template<typename Key, typename Value, typename = std::enable_if_t<is_symmetry<Key> || is_symmetry_vector_v<Key>>>
   std::istream& operator>(std::istream& in, std::map<Key, Value>& map) {
      map.clear();
      Size size;
      in > size;
      for (Size i = 0; i < size; i++) {
         Key key;
         in > key;
         in > map[std::move(key)];
      }
      return in;
   }

   template<typename T, typename A, typename = std::enable_if_t<is_scalar<T> || is_edge<T> || is_symmetry<T> || is_name<T>>>
   std::ostream& operator<<(std::ostream& out, const std::vector<T, A>& list) {
      out << '[';
      auto not_first = false;
      for (const auto& i : list) {
         if (not_first) {
            out << ',';
         }
         not_first = true;
         if constexpr (is_name<T>) {
            NameTraits<T>::print(out, i);
         } else if constexpr (std::is_same_v<T, std::complex<real_scalar<T>>>) {
            detail::print_complex(out, i);
         } else {
            out << i;
         }
      }
      out << ']';
      return out;
   }

   template<typename T, typename A, typename = std::enable_if_t<is_scalar<T> || is_edge<T> || is_symmetry<T> || is_name<T>>>
   std::istream& operator>>(std::istream& in, std::vector<T, A>& list) {
      list.clear();
      detail::ignore_until(in, '[');
      if (in.peek() == ']') {
         // empty list
         in.get(); // get ']'
      } else {
         // not empty
         while (true) {
            // no space here
            auto& i = list.emplace_back();
            if constexpr (is_name<T>) {
               NameTraits<T>::scan(in, i);
            } else if constexpr (std::is_same_v<T, std::complex<real_scalar<T>>>) {
               detail::scan_complex(in, i);
            } else {
               in >> i;
            }
            char next = in.get();
            if (next == ']') {
               break;
            }
         }
      }
      return in;
   }

   // edge io

   template<typename Symmetry>
   std::ostream& operator<<(std::ostream& out, const Edge<Symmetry>& edge) {
      if constexpr (Symmetry::length == 0) {
         out << edge.segment.front().second;
      } else {
         if constexpr (Symmetry::is_fermi_symmetry) {
            out << '{';
            out << "arrow" << ':';
            out << edge.arrow;
            out << ',';
            out << "segment" << ':';
         }
         out << '{';
         auto not_first = false;
         for (const auto& [symmetry, dimension] : edge.segment) {
            if (not_first) {
               out << ',';
            }
            not_first = true;
            out << symmetry << ':' << dimension;
         }
         out << '}';
         if constexpr (Symmetry::is_fermi_symmetry) {
            out << '}';
         }
      }
      return out;
   }
   template<typename Symmetry>
   std::istream& operator>>(std::istream& in, Edge<Symmetry>& edge) {
      if constexpr (Symmetry::length == 0) {
         edge.segment.clear();
         in >> edge.segment.emplace_back(Symmetry(), 0).second;
      } else {
         if constexpr (Symmetry::is_fermi_symmetry) {
            detail::ignore_until(in, ':');
            in >> edge.arrow;
         }
         edge.segment.clear();
         detail::ignore_until(in, '{');
         if (in.peek() != '}') {
            // not empty
            do {
               Symmetry symmetry;
               in >> symmetry;
               detail::ignore_until(in, ':');
               Size dimension;
               in >> dimension;
               edge.segment.emplace_back(symmetry, dimension);
            } while (in.get() == ','); // read last '}' of segment
         } else {
            in.get(); // read last '}' of segment
         }
         if constexpr (Symmetry::is_fermi_symmetry) {
            detail::ignore_until(in, '}');
         }
      }
      return in;
   }

   template<typename Symmetry>
   std::ostream& operator<(std::ostream& out, const Edge<Symmetry>& edge) {
      if constexpr (Symmetry::is_fermi_symmetry) {
         out < edge.arrow;
      }
      out < edge.segment;
      return out;
   }
   template<typename Symmetry>
   std::istream& operator>(std::istream& in, Edge<Symmetry>& edge) {
      if constexpr (Symmetry::is_fermi_symmetry) {
         in > edge.arrow;
      }
      in > edge.segment;
      return in;
   }

   // symmetry io, text only, bin can use direct

   namespace detail {
      template<typename Symmetry, std::size_t... Is>
      void print_symmetry_sequence(std::ostream& out, const Symmetry& symmetry, std::index_sequence<Is...>) {
         (((Is == 0 ? out : out << ',') << std::get<Is>(symmetry)), ...);
      }
   } // namespace detail
   template<typename... T>
   std::ostream& operator<<(std::ostream& out, const Symmetry<T...>& symmetry) {
      using Symmetry = Symmetry<T...>;
      if constexpr (Symmetry::length != 0) {
         if constexpr (Symmetry::length == 1) {
            out << std::get<0>(symmetry);
         } else {
            out << '(';
            detail::print_symmetry_sequence(out, symmetry, typename Symmetry::index_sequence_t());
            out << ')';
         }
      }
      return out;
   }
   namespace detail {
      template<typename Symmetry, std::size_t... Is>
      void scan_symmetry_sequence(std::istream& in, Symmetry& symmetry, std::index_sequence<Is...>) {
         (((Is == 0 ? in : (detail::ignore_until(in, ','), in)) >> std::get<Is>(symmetry)), ...);
      }
   } // namespace detail
   template<typename... T>
   std::istream& operator>>(std::istream& in, Symmetry<T...>& symmetry) {
      using Symmetry = Symmetry<T...>;
      if constexpr (Symmetry::length != 0) {
         if constexpr (Symmetry::length == 1) {
            in >> std::get<0>(symmetry);
         } else {
            detail::ignore_until(in, '(');
            detail::scan_symmetry_sequence(in, symmetry, typename Symmetry::index_sequence_t());
            detail::ignore_until(in, ')');
         }
      }
      return in;
   }

   /**
    * A type control console color
    */
   struct UnixColorCode {
      std::string color_code;
      UnixColorCode(const char* code) noexcept : color_code(code) {}
   };
   inline const UnixColorCode console_red = "\x1B[31m";
   inline const UnixColorCode console_green = "\x1B[32m";
   inline const UnixColorCode console_yellow = "\x1B[33m";
   inline const UnixColorCode console_blue = "\x1B[34m";
   inline const UnixColorCode console_origin = "\x1B[0m";
   inline std::ostream& operator<<(std::ostream& out, const UnixColorCode& value) {
      out << value.color_code;
      return out;
   }

   // tensor shape, text output only

   template<
         typename ScalarType,
         typename Symmetry,
         typename Name,
         typename = std::enable_if_t<is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>>>
   std::ostream& operator<<(std::ostream& out, const TensorShape<ScalarType, Symmetry, Name>& shape) {
      const auto& tensor = *shape.owner;
      out << '{' << console_green << "names" << console_origin << ':';
      out << tensor.names << ',';
      out << console_green << "edges" << console_origin << ':';
      out << tensor.core->edges << '}';
      return out;
   }

   // tensor text in out

   template<
         typename ScalarType,
         typename Symmetry,
         typename Name,
         typename = std::enable_if_t<is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>>>
   std::ostream& operator<<(std::ostream& out, const Tensor<ScalarType, Symmetry, Name>& tensor) {
      out << '{' << console_green << "names" << console_origin << ':';
      out << tensor.names << ',';
      out << console_green << "edges" << console_origin << ':';
      out << tensor.core->edges << ',';
      out << console_green << "blocks" << console_origin << ':';
      if constexpr (Symmetry::length == 0) {
         out << tensor.core->blocks.begin()->second;
      } else {
         out << '{';
         auto not_first = false;
         for (const auto& [symmetries, block] : tensor.core->blocks) {
            if (not_first) {
               out << ',';
            }
            not_first = true;
            out << console_yellow << symmetries << console_origin << ':' << block;
         }
         out << '}';
      }
      out << '}';
      return out;
   }

   template<
         typename ScalarType,
         typename Symmetry,
         typename Name,
         typename = std::enable_if_t<is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>>>
   std::istream& operator>>(std::istream& in, Tensor<ScalarType, Symmetry, Name>& tensor) {
      detail::ignore_until(in, ':');
      in >> tensor.names;
      detail::ignore_until(in, ':');
      std::vector<Edge<Symmetry>> edges;
      in >> edges;
      tensor.core = std::make_shared<Core<ScalarType, Symmetry>>(std::move(edges));
      if constexpr (debug_mode) {
         tensor.check_valid_name();
      }
      detail::ignore_until(in, ':');
      if constexpr (Symmetry::length == 0) {
         // change begin();
         auto& block = tensor.core->blocks.front().second;
         block.clear(); // clear vector，storage not change
         in >> block;
      } else {
         detail::ignore_until(in, '{');
         if (in.peek() != '}') {
            do {
               std::vector<Symmetry> symmetries;
               in >> symmetries;
               detail::ignore_until(in, ':');
               auto& block = tensor.blocks(symmetries);
               block.clear();
               in >> block;
            } while (in.get() == ','); // read last '}' of map
         } else {
            in.get(); // read last '}' of map
         }
      }
      detail::ignore_until(in, '}');
      return in;
   }

   template<typename ScalarType, typename Symmetry, typename Name>
   std::string Tensor<ScalarType, Symmetry, Name>::show() const {
      std::ostringstream out;
      out << *this;
      return out.str();
   }

   // tensor bin out

   template<typename ScalarType, typename Symmetry, typename Name>
   const Tensor<ScalarType, Symmetry, Name>& Tensor<ScalarType, Symmetry, Name>::meta_put(std::ostream& out) const {
      out < names;
      out < core->edges;
      return *this;
   }

   template<typename ScalarType, typename Symmetry, typename Name>
   const Tensor<ScalarType, Symmetry, Name>& Tensor<ScalarType, Symmetry, Name>::data_put(std::ostream& out) const {
      out < storage();
      return *this;
   }

   template<
         typename ScalarType,
         typename Symmetry,
         typename Name,
         typename = std::enable_if_t<is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>>>
   std::ostream& operator<(std::ostream& out, const Tensor<ScalarType, Symmetry, Name>& tensor) {
      tensor.meta_put(out).data_put(out);
      return out;
   }

   template<typename ScalarType, typename Symmetry, typename Name>
   std::string Tensor<ScalarType, Symmetry, Name>::dump() const {
      std::ostringstream out;
      out < *this;
      return out.str();
   }

   // tensor bin in

   template<typename ScalarType, typename Symmetry, typename Name>
   Tensor<ScalarType, Symmetry, Name>& Tensor<ScalarType, Symmetry, Name>::meta_get(std::istream& in) {
      in > names;
      std::vector<Edge<Symmetry>> edges;
      in > edges;
      core = std::make_shared<Core<ScalarType, Symmetry>>(std::move(edges));
      return *this;
   }

   template<typename ScalarType, typename Symmetry, typename Name>
   Tensor<ScalarType, Symmetry, Name>& Tensor<ScalarType, Symmetry, Name>::data_get(std::istream& in) {
      in > storage();
      return *this;
   }

   template<
         typename ScalarType,
         typename Symmetry,
         typename Name,
         typename = std::enable_if_t<is_scalar<ScalarType> && is_symmetry<Symmetry> && is_name<Name>>>
   std::istream& operator>(std::istream& in, Tensor<ScalarType, Symmetry, Name>& tensor) {
      tensor.meta_get(in).data_get(in);
      if constexpr (debug_mode) {
         tensor.check_valid_name();
      }
      return in;
   }

   template<typename ScalarType, typename Symmetry, typename Name>
   Tensor<ScalarType, Symmetry, Name>& Tensor<ScalarType, Symmetry, Name>::load(const std::string& input) & {
      std::istringstream in(input);
      in > *this;
      return *this;
   }

   // fast name dataset

   inline std::ostream& operator<(std::ostream& out, const FastName::dataset_t& dataset) {
      return out < dataset.id_to_name;
   }
   inline std::istream& operator>(std::istream& in, FastName::dataset_t& dataset) {
      in > dataset.id_to_name;
      dataset.fastname_number = dataset.id_to_name.size();
      dataset.name_to_id.clear();
      for (auto i = 0; i < dataset.fastname_number; i++) {
         dataset.name_to_id[dataset.id_to_name[i]] = i;
      }
      return in;
   }
   inline void load_fastname_dataset(const std::string& input) {
      std::istringstream in(input);
      in > FastName::dataset;
   }
   inline std::string dump_fastname_dataset() {
      std::ostringstream out;
      out < FastName::dataset;
      return out.str();
   }

   // binary io move

   template<typename T>
   std::istream&& operator>(std::istream&& in, T& v) {
      in > v;
      return std::move(in);
   }
   template<typename T>
   std::ostream&& operator<(std::ostream&& out, const T& v) {
      out < v;
      return std::move(out);
   }
} // namespace TAT
#endif
