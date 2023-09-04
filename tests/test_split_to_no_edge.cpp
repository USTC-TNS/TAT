#include <TAT/TAT.hpp>
#include <gtest/gtest.h>

TEST(test_split_to_no_edge, basic_usage_0) {
   auto a = TAT::Tensor<float, TAT::Z2Symmetry>({"i", "j"}, {{{false, 2}}, {{false, 1}}}).range();
   auto b = a.split_edge({
         {"i", {{"k", {std::vector<std::pair<TAT::Z2Symmetry, TAT::Size>>{{false, 2}}}}}},
         {"j", {}},
   });
}

TEST(test_split_to_no_edge, basic_usage_1) {
   auto a = TAT::Tensor<float, TAT::Z2Symmetry>({"i", "j"}, {{{false, 2}, {true, 2}}, {{false, 1}}}).range();
   auto b = a.split_edge({
         {"i", {{"k", {std::vector<std::pair<TAT::Z2Symmetry, TAT::Size>>{{false, 2}, {true, 2}}}}}},
         {"j", {}},
   });
}
