#include <TAT/TAT.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;

#define edge_t(...) \
    { {__VA_ARGS__}, true }
#define edge_f(...) \
    { {__VA_ARGS__}, false }

TEST(test_create_fermi_tensor, basic_usage) {
    // 1 1 0 : 3*1*3
    // 1 0 1 : 3*2*2
    // 0 1 1 : 1*1*2
    // 0 0 0 : 1*2*3
    auto a = (TAT::Tensor<double, TAT::ParitySymmetry>{
        {"Left", "Right", "Up"},
        {edge_t({1, 3}, {0, 1}), edge_f({1, 1}, {0, 2}), edge_t({1, 2}, {0, 3})},
    }
                  .range());
    ASSERT_EQ(a.names(0), "Left");
    ASSERT_EQ(a.names(1), "Right");
    ASSERT_EQ(a.names(2), "Up");
    ASSERT_THAT(a.names(), ElementsAre("Left", "Right", "Up"));
    ASSERT_EQ(a.rank_by_name("Left"), 0);
    ASSERT_EQ(a.rank_by_name("Right"), 1);
    ASSERT_EQ(a.rank_by_name("Up"), 2);
    ASSERT_EQ(a.storage().size(), 1 * 2 * 3 + 1 * 1 * 2 + 3 * 2 * 2 + 3 * 1 * 3);
    ASSERT_EQ(&a.edges("Left"), &a.edges(0));
    ASSERT_EQ(&a.edges("Right"), &a.edges(1));
    ASSERT_EQ(&a.edges("Up"), &a.edges(2));
    ASSERT_EQ(a.edges(0).arrow(), true);
    ASSERT_EQ(a.edges(1).arrow(), false);
    ASSERT_EQ(a.edges(2).arrow(), true);

    ASSERT_THAT(a.blocks(std::vector<int>{0, 0, 1}).dimensions(), ElementsAre(3, 1, 3));
    ASSERT_THAT(a.const_blocks(std::vector<int>{1, 1, 1}).dimensions(), ElementsAre(1, 2, 3));
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, int>{{"Left", 0}, {"Right", 1}, {"Up", 0}}).dimensions(), ElementsAre(3, 2, 2));
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, int>{{"Left", 1}, {"Right", 0}, {"Up", 0}}).dimensions(), ElementsAre(1, 1, 2));
    ASSERT_THAT(a.blocks(std::vector<TAT::ParitySymmetry>{1, 1, 0}).dimensions(), ElementsAre(3, 1, 3));
    ASSERT_THAT(a.const_blocks(std::vector<TAT::ParitySymmetry>{0, 0, 0}).dimensions(), ElementsAre(1, 2, 3));
    ASSERT_THAT(
        a.blocks(std::unordered_map<std::string, TAT::ParitySymmetry>{{"Left", 1}, {"Right", 0}, {"Up", 1}}).dimensions(),
        ElementsAre(3, 2, 2)
    );
    ASSERT_THAT(
        a.const_blocks(std::unordered_map<std::string, TAT::ParitySymmetry>{{"Left", 0}, {"Right", 1}, {"Up", 1}}).dimensions(),
        ElementsAre(1, 1, 2)
    );

    ASSERT_FLOAT_EQ(a.at(std::vector<std::pair<TAT::ParitySymmetry, TAT::Size>>{{1, 1}, {1, 0}, {0, 2}}), 5);
    ASSERT_FLOAT_EQ(
        a.at(std::unordered_map<std::string, std::pair<TAT::ParitySymmetry, TAT::Size>>{{"Left", {1, 2}}, {"Right", {0, 0}}, {"Up", {1, 1}}}),
        3 * 1 * 3 + 9
    );
    ASSERT_FLOAT_EQ(a.const_at(std::vector<std::pair<TAT::ParitySymmetry, TAT::Size>>{{0, 0}, {1, 0}, {1, 1}}), 3 * 1 * 3 + 3 * 2 * 2 + 1);
    ASSERT_FLOAT_EQ(
        a.const_at(std::unordered_map<std::string, std::pair<TAT::ParitySymmetry, TAT::Size>>{{"Left", {0, 0}}, {"Right", {0, 1}}, {"Up", {0, 2}}}),
        3 * 1 * 3 + 3 * 2 * 2 + 1 * 1 * 2 + 5
    );
}

TEST(test_create_fermi_tensor, when_0rank) {
    auto a = TAT::Tensor<double, TAT::FermiSymmetry>{{}, {}}.range(2333);
    ASSERT_THAT(a.names(), ElementsAre());
    ASSERT_THAT(a.storage(), ElementsAre(2333));

    ASSERT_THAT(a.blocks(std::vector<int>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.const_blocks(std::vector<int>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, int>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, int>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.blocks(std::vector<TAT::FermiSymmetry>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.const_blocks(std::vector<TAT::FermiSymmetry>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, TAT::FermiSymmetry>{}).dimensions(), ElementsAre());
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, TAT::FermiSymmetry>{}).dimensions(), ElementsAre());

    ASSERT_FLOAT_EQ(a.at(std::vector<TAT::Size>{}), 2333);
    ASSERT_FLOAT_EQ(a.at(std::unordered_map<std::string, TAT::Size>{}), 2333);
    ASSERT_FLOAT_EQ(a.const_at(std::vector<TAT::Size>{}), 2333);
    ASSERT_FLOAT_EQ(a.const_at(std::unordered_map<std::string, TAT::Size>{}), 2333);
}

TEST(test_create_fermi_tensor, when_0size) {
    using sym_t = TAT::FermiSymmetry;
    auto a = (TAT::Tensor<double, sym_t>({"Left", "Right", "Up"}, {edge_f({0, 0}), edge_t({-1, 1}, {0, 2}, {1, 3}), edge_t({-1, 2}, {0, 3}, {1, 1})})
                  .zero());
    ASSERT_EQ(a.names(0), "Left");
    ASSERT_EQ(a.names(1), "Right");
    ASSERT_EQ(a.names(2), "Up");
    ASSERT_THAT(a.names(), ElementsAre("Left", "Right", "Up"));
    ASSERT_EQ(a.rank_by_name("Left"), 0);
    ASSERT_EQ(a.rank_by_name("Right"), 1);
    ASSERT_EQ(a.rank_by_name("Up"), 2);
    ASSERT_THAT(a.storage(), ElementsAre());
    ASSERT_EQ(&a.edges("Left"), &a.edges(0));
    ASSERT_EQ(&a.edges("Right"), &a.edges(1));
    ASSERT_EQ(&a.edges("Up"), &a.edges(2));
    ASSERT_EQ(a.edges(0).arrow(), false);
    ASSERT_EQ(a.edges(1).arrow(), true);
    ASSERT_EQ(a.edges(2).arrow(), true);

    ASSERT_THAT(a.blocks(std::vector<int>{0, 1, 1}).dimensions(), ElementsAre(0, 2, 3));
    ASSERT_THAT(a.const_blocks(std::vector<int>{0, 0, 2}).dimensions(), ElementsAre(0, 1, 1));
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, int>{{"Left", 0}, {"Right", 2}, {"Up", 0}}).dimensions(), ElementsAre(0, 3, 2));
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, int>{{"Left", 0}, {"Right", 2}, {"Up", 0}}).dimensions(), ElementsAre(0, 3, 2));
    ASSERT_THAT(a.blocks(std::vector<sym_t>{0, 0, 0}).dimensions(), ElementsAre(0, 2, 3));
    ASSERT_THAT(a.const_blocks(std::vector<sym_t>{0, -1, +1}).dimensions(), ElementsAre(0, 1, 1));
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, sym_t>{{"Left", 0}, {"Right", +1}, {"Up", -1}}).dimensions(), ElementsAre(0, 3, 2));
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, sym_t>{{"Left", 0}, {"Right", +1}, {"Up", -1}}).dimensions(), ElementsAre(0, 3, 2));
}

TEST(test_create_fermi_tensor, when_0block) {
    using sym_t = TAT::FermiSymmetry;
    auto a = (TAT::Tensor<double, sym_t>(
                  {"Left", "Right", "Up"},
                  {{std::vector<sym_t>(), false}, edge_f({-1, 1}, {0, 2}, {1, 3}), edge_t({-1, 2}, {0, 3}, {1, 1})}
    )
                  .zero());
    ASSERT_EQ(a.names(0), "Left");
    ASSERT_EQ(a.names(1), "Right");
    ASSERT_EQ(a.names(2), "Up");
    ASSERT_THAT(a.names(), ElementsAre("Left", "Right", "Up"));
    ASSERT_EQ(a.rank_by_name("Left"), 0);
    ASSERT_EQ(a.rank_by_name("Right"), 1);
    ASSERT_EQ(a.rank_by_name("Up"), 2);
    ASSERT_THAT(a.storage(), ElementsAre());
    ASSERT_EQ(&a.edges("Left"), &a.edges(0));
    ASSERT_EQ(&a.edges("Right"), &a.edges(1));
    ASSERT_EQ(&a.edges("Up"), &a.edges(2));
    ASSERT_EQ(a.edges(0).arrow(), false);
    ASSERT_EQ(a.edges(1).arrow(), false);
    ASSERT_EQ(a.edges(2).arrow(), true);
}

TEST(test_create_fermi_tensor, conversion_scalar) {
    auto a = TAT::Tensor<double, TAT::FermiSymmetry>(2333, {"i", "j"}, {-2, +2}, {true, false});
    ASSERT_EQ(a.names(0), "i");
    ASSERT_EQ(a.names(1), "j");
    ASSERT_THAT(a.names(), ElementsAre("i", "j"));
    ASSERT_EQ(a.rank_by_name("i"), 0);
    ASSERT_EQ(a.rank_by_name("j"), 1);
    ASSERT_THAT(a.storage(), ElementsAre(2333));
    ASSERT_EQ(&a.edges("i"), &a.edges(0));
    ASSERT_EQ(&a.edges("j"), &a.edges(1));
    ASSERT_EQ(a.edges(0).arrow(), true);
    ASSERT_EQ(a.edges(1).arrow(), false);

    ASSERT_THAT(a.blocks(std::vector<int>{0, 0}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.const_blocks(std::vector<int>{0, 0}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, int>{{"i", 0}, {"j", 0}}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, int>{{"i", 0}, {"j", 0}}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.blocks(std::vector<TAT::FermiSymmetry>{-2, +2}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.const_blocks(std::vector<TAT::FermiSymmetry>{-2, +2}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.blocks(std::unordered_map<std::string, TAT::FermiSymmetry>{{"i", -2}, {"j", +2}}).dimensions(), ElementsAre(1, 1));
    ASSERT_THAT(a.const_blocks(std::unordered_map<std::string, TAT::FermiSymmetry>{{"i", -2}, {"j", +2}}).dimensions(), ElementsAre(1, 1));

    ASSERT_FLOAT_EQ(a.at(), 2333);
    ASSERT_FLOAT_EQ(a.const_at(), 2333);
    ASSERT_FLOAT_EQ(double(a), 2333);
}

TEST(test_create_fermi_tensor, conversion_scalar_empty) {
    auto a = TAT::Tensor<double, TAT::FermiSymmetry>({"i"}, {{{+1, 2333}}}).range(2333);
    ASSERT_FLOAT_EQ(double(a), 0);
}
