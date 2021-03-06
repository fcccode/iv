#include <gtest/gtest.h>
#include <iv/character.h>

TEST(CharacterCase, CategoryTest) {
  using iv::core::character::GetCategory;
  for (char16_t i = 0; i < 0xffff; ++i) {
    GetCategory(i);
  }
  ASSERT_EQ(static_cast<uint32_t>(iv::core::character::UNASSIGNED), GetCategory(0x037F));
}

TEST(CharacterCase, ToUpperCaseTest) {
  using iv::core::character::ToUpperCase;
  // for ascii test
  for (char16_t ch = 'a',
       target = 'A'; ch <= 'z'; ++ch, ++target) {
    ASSERT_EQ(ToUpperCase(ch), target);
  }
  ASSERT_EQ(0x00CBu, ToUpperCase(0x00EB));
  ASSERT_EQ(0x0531u, ToUpperCase(0x0561));
  ASSERT_EQ(static_cast<char16_t>('A'), ToUpperCase('A'));
  ASSERT_EQ(0x0560u, ToUpperCase(0x0560));

  for (uint32_t ch = 0; ch < 0x10000; ++ch) {
    ToUpperCase(ch);
  }
  ASSERT_EQ(0x00530053u, ToUpperCase(0x00DF));
  ASSERT_EQ(0x00460046u, ToUpperCase(0xFB00));
}

TEST(CharacterCase, ToLowerCaseTest) {
  using iv::core::character::ToLowerCase;
  // for ascii test
  for (char16_t ch = 'A',
       target = 'a'; ch <= 'A'; ++ch, ++target) {
    ASSERT_EQ(ToLowerCase(ch), target);
  }
  ASSERT_EQ(0x0561u, ToLowerCase(0x0531));
  ASSERT_EQ(0x0560u, ToLowerCase(0x0560));

  for (uint32_t ch = 0; ch < 0x10000; ++ch) {
    ToLowerCase(ch);
  }
}
