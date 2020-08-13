#include "xcdrTypeSupportImpl.h"

#include <dds/DCPS/Serializer.h>
#include <dds/DCPS/SafetyProfileStreams.h>

#include <gtest/gtest.h>

#include <string>
#include <cstring>

using namespace OpenDDS::DCPS;

const Encoding xcdr1(Encoding::KIND_XCDR1, ENDIAN_BIG);
const Encoding xcdr2(Encoding::KIND_XCDR2, ENDIAN_BIG);
// Big Endian just so it's easier to write expected CDR

template <typename Type>
void set_base_values(Type& value)
{
  value.short_field = 0x7fff;
  value.long_field = 0x7fffffff;
  value.octet_field = 0x01;
  value.long_long_field = 0x7fffffffffffffff;
}

template <typename Type>
void set_values(Type& value)
{
  set_base_values(value);
}

template <>
void set_values<AdditionalFieldMutableStruct>(
  AdditionalFieldMutableStruct& value)
{
  set_base_values(value);
  value.additional_field = 0x12345678;
}

template<typename TypeA, typename TypeB>
void expect_values_equal_base(const TypeA& a, const TypeB& b)
{
  EXPECT_EQ(a.short_field, b.short_field);
  EXPECT_EQ(a.long_field, b.long_field);
  EXPECT_EQ(a.octet_field, b.octet_field);
  EXPECT_EQ(a.long_long_field, b.long_long_field);
}

template<typename TypeA, typename TypeB>
void expect_values_equal(const TypeA& a, const TypeB& b)
{
  expect_values_equal_base(a, b);
}

template<typename TypeA, typename TypeB>
::testing::AssertionResult assert_values(
  const char* a_expr, const char* b_expr,
  const TypeA& a, const TypeB& b)
{
  expect_values_equal(a, b);
  if (::testing::Test::HasFailure()) {
    return ::testing::AssertionFailure() << a_expr << " != " << b_expr;
  }
  return ::testing::AssertionSuccess();
}

struct DataView {
  template <size_t array_size>
  DataView(const unsigned char (&array)[array_size])
  : data(reinterpret_cast<const char*>(&array[0]))
  , size(array_size)
  {
  }

  DataView(const ACE_Message_Block& mb)
  : data(mb.base())
  , size(mb.length())
  {
  }

  const char* const data;
  const size_t size;
};

bool operator==(const DataView& a, const DataView& b)
{
  return a.size == b.size ? !std::memcmp(a.data, b.data, a.size) : false;
}

::testing::AssertionResult assert_DataView(
  const char* a_expr, const char* b_expr,
  const DataView& a, const DataView& b)
{
  if (a == b) {
    return ::testing::AssertionSuccess();
  }

  std::string a_line;
  std::stringstream astrm(to_hex_dds_string(a.data, a.size, '\n', 8));
  std::string b_line;
  std::stringstream bstrm(to_hex_dds_string(b.data, b.size, '\n', 8));
  std::stringstream result;
  bool got_a = true;
  bool got_b = true;
  bool diff = true;
  while (got_a || got_b) {
    result.width(16);
    result << std::left;
    if (got_a && std::getline(astrm, a_line, '\n')) {
      result << a_line;
    } else {
      got_a = false;
      result << ' ';
    }
    if (got_b && std::getline(bstrm, b_line, '\n')) {
      result << " " << b_line;
    } else {
      got_b = false;
    }
    if (diff && a_line != b_line) {
      result << " <- Different Starting Here";
      diff = false;
    }
    result << std::endl;
  }
  return ::testing::AssertionFailure()
    << a_expr << " (size " << a.size << ") isn't the same as "
    << b_expr << " (size " << b.size << ").\n"
    << a_expr << " is on the left, " << b_expr << " is on the right:\n"
    << result.str();
}

template<typename TypeA, typename TypeB>
void amalgam_serializer_test(
  const Encoding& encoding, const DataView& expected_cdr, TypeA& value, TypeB& result)
{
  ACE_Message_Block buffer(1024);

  // Serialize and Compare CDR
  {
    Serializer serializer(&buffer, encoding);
    ASSERT_TRUE(serializer << value);
    EXPECT_PRED_FORMAT2(assert_DataView, expected_cdr, buffer);
  }

  // Deserialize and Compare C++ Values
  {
    Serializer serializer(&buffer, encoding);
    ASSERT_TRUE(serializer >> result);
    EXPECT_PRED_FORMAT2(assert_values, value, result);
  }
}

template<typename TypeA, typename TypeB>
void amalgam_serializer_test(const Encoding& encoding, const DataView& expected_cdr)
{
  TypeA value;
  set_values(value);
  TypeB result;
  amalgam_serializer_test<TypeA, TypeB>(encoding, expected_cdr, value, result);
}

template<typename Type>
void serializer_test(const Encoding& encoding, const DataView& expected_cdr)
{
  amalgam_serializer_test<Type, Type>(encoding, expected_cdr);
}

template<typename Type>
void baseline_checks(const Encoding& encoding, const DataView& expected_cdr)
{
  Type value;
  EXPECT_EQ(serialized_size(encoding, value), expected_cdr.size);
  // TODO(iguessthislldo): Does not work for XCDR1 and XCDR2 Mutable and XCDR2 Appendable
  // EXPECT_EQ(max_serialized_size(encoding, value), expected_cdr.size);

  serializer_test<Type>(encoding, expected_cdr);
}

// XCDR1 =====================================================================

const unsigned char final_xcdr1_struct_expected[] = {
  // short_field
  0x7f, 0xff, // +2 = 2
  // long_field
  0x00, 0x00, // +2 pad = 4
  0x7f, 0xff, 0xff, 0xff, // +4 = 8
  // octet_field
  0x01, // +1 = 9
  // long_long_field
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // +7 pad = 16
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 24
};

TEST(basic_tests, FinalXcdr1Struct)
{
  baseline_checks<FinalXcdr1Struct>(xcdr1, final_xcdr1_struct_expected);
}

const DataView appendable_xcdr1_struct_expected(final_xcdr1_struct_expected);

TEST(basic_tests, AppendableXcdr1Struct)
{
  baseline_checks<AppendableXcdr1Struct>(xcdr1, appendable_xcdr1_struct_expected);
}

const unsigned char mutable_xcdr1_struct_expected[] = {
  // short_field
  0xc0, 0x00, 0x00, 0x02, // PID +4 = 4
  0x7f, 0xff, // +2 = 6
  // long_field
  0x00, 0x00, // +2 pad = 8
  0xc0, 0x01, 0x00, 0x04, // PID +4 = 12
  0x7f, 0xff, 0xff, 0xff, // +4 = 16
  // octet_field
  0xc0, 0x02, 0x00, 0x01, // PID +4 = 20
  0x01, // +1 = 21
  // long_long_field
  0x00, 0x00, 0x00, // +3 pad = 24
  0xc0, 0x03, 0x00, 0x08, // PID +4 = 28
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 36
  // End of List
  0x3f, 0x02, 0x00, 0x00 // +4 = 40
};

TEST(basic_tests, MutableXcdr1Struct)
{
  baseline_checks<MutableXcdr1Struct>(xcdr1, mutable_xcdr1_struct_expected);
}

// XCDR2 =====================================================================

const unsigned char final_xcdr2_struct_expected[] = {
  // short_field
  0x7f, 0xff, // +2 = 2
  // long_field
  0x00, 0x00, // +2 pad = 4
  0x7f, 0xff, 0xff, 0xff, // +4 = 8
  // octet_field
  0x01, // +1 = 9
  // long_long_field
  0x00, 0x00, 0x00, // +3 pad = 12
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff // +8 = 20
};

TEST(basic_tests, FinalXcdr2Struct)
{
  baseline_checks<FinalXcdr2Struct>(xcdr2, final_xcdr2_struct_expected);
}

const unsigned char appendable_xcdr2_struct_expected[] = {
  // Delimiter
  0x00, 0x00, 0x00, 0x14, // +4 = 4
  // short_field
  0x7f, 0xff, // +2 = 6
  // long_field
  0x00, 0x00, // +2 pad = 8
  0x7f, 0xff, 0xff, 0xff, // +4 = 12
  // octet_field
  0x01, // +1 = 13
  // long_long_field
  0x00, 0x00, 0x00, // +3 pad = 16
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff // +8 = 24
};

TEST(basic_tests, AppendableXcdr2Struct)
{
  baseline_checks<AppendableXcdr2Struct>(xcdr2, appendable_xcdr2_struct_expected);
}

const unsigned char mutable_xcdr2_struct_expected[] = {
  // Delimiter
  0x00, 0x00, 0x00, 0x24, // +4 = 4
  // short_field
  0x00, 0x00, 0x00, 0x00, // PID  +4 = 8
  0x7f, 0xff, // +2 = 10
  // long_field
  0x00, 0x00, // +2 pad = 12
  0x00, 0x00, 0x00, 0x01, // PID  +4 = 16
  0x7f, 0xff, 0xff, 0xff, // +4 = 20
  // octet_field
  0x00, 0x00, 0x00, 0x02, // PID  +4 = 24
  0x01, // +1 = 25
  // long_long_field
  0x00, 0x00, 0x00, // +3 pad = 28
  0x00, 0x00, 0x00, 0x03, // PID  +4 = 32
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff // +8 = 40
};
/*
TEST(basic_tests, MutableXcdr2Struct)
{
  baseline_checks<MutableXcdr2Struct>(xcdr2, mutable_xcdr2_struct_expected);
}

// XCDR1 and XCDR2 ===========================================================

TEST(basic_tests, FinalXcdr12Struct)
{
  baseline_checks<FinalXcdr12Struct>(xcdr1, final_xcdr1_struct_expected);
  baseline_checks<FinalXcdr12Struct>(xcdr2, final_xcdr2_struct_expected);
}

TEST(basic_tests, AppendableXcdr12Struct)
{
  baseline_checks<AppendableXcdr12Struct>(xcdr1, appendable_xcdr1_struct_expected);
  baseline_checks<AppendableXcdr12Struct>(xcdr2, appendable_xcdr2_struct_expected);
}

TEST(basic_tests, MutableXcdr12Struct)
{
  baseline_checks<MutableXcdr12Struct>(xcdr1, mutable_xcdr1_struct_expected);
  baseline_checks<MutableXcdr12Struct>(xcdr2, mutable_xcdr2_struct_expected);
}

// Appendable Tests ==========================================================
// TODO(iguessthislldo)

// Mutable Tests =============================================================

const unsigned char mutable_struct_expected_xcdr1[] = {
  // short_field
  0xc0, 0x04, 0x00, 0x02, // PID +4 = 4
  0x7f, 0xff, // +2 = 6
  // long_field
  0x00, 0x00, // +2 pad = 8
  0xc0, 0x06, 0x00, 0x04, // PID +4 = 12
  0x7f, 0xff, 0xff, 0xff, // +4 = 16
  // octet_field
  0xc0, 0x08, 0x00, 0x01, // PID +4 = 20
  0x01, // +1 = 21
  // long_long_field
  0x00, 0x00, 0x00, // +3 pad = 24
  0xc0, 0x0a, 0x00, 0x08, // PID +4 = 28
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 36
  // End of List
  0x3f, 0x02, 0x00, 0x00 // +4 = 40
};

const unsigned char mutable_struct_expected_xcdr2[] = {
  // Delimiter
  0x00, 0x00, 0x00, 0x24, // +4 = 4
  // short_field
  0x00, 0x00, 0x00, 0x04, // PID +4 = 8
  0x7f, 0xff, // +2 = 10
  // long_field
  0x00, 0x00, // +2 pad = 12
  0x00, 0x00, 0x00, 0x06, // PID +4 = 16
  0x7f, 0xff, 0xff, 0xff, // +4 = 20
  // octet_field
  0x00, 0x00, 0x00, 0x08, // PID +4 = 24
  0x01, // +1 = 25
  // long_long_field
  0x00, 0x00, 0x00, // +3 pad = 28
  0x00, 0x00, 0x00, 0x0a, // PID +4 = 32
  0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff // +8 = 40
};

// Baseline ------------------------------------------------------------------

TEST(mutable_tests, baseline_xcdr1_test)
{
  baseline_checks<MutableStruct>(xcdr1, mutable_struct_expected_xcdr1);
}

TEST(mutable_tests, baseline_xcdr2_test)
{
  baseline_checks<MutableStruct>(xcdr2, mutable_struct_expected_xcdr2);
}

// Reordered Tests -----------------------------------------------------------
// Test compatibility between two structures with different field orders.

TEST(mutable_tests, to_reordered_xcdr1_test)
{
  amalgam_serializer_test<MutableStruct, ReorderedMutableStruct>(
    xcdr1, mutable_struct_expected_xcdr1);
}

TEST(mutable_tests, from_reordered_xcdr1_test)
{
  const unsigned char expected[] = {
    // long_field
    0xc0, 0x06, 0x00, 0x04, // PID +4 = 4
    0x7f, 0xff, 0xff, 0xff, // +4 = 8
    // long_long_field
    0xc0, 0x0a, 0x00, 0x08, // PID +4 = 12
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 20
    // octet_field
    0xc0, 0x08, 0x00, 0x01, // PID +4 = 24
    0x01, // +1 = 25
    // short_field
    0x00, 0x00, 0x00, // +3 pad = 28
    0xc0, 0x04, 0x00, 0x02, // PID +4 = 32
    0x7f, 0xff, // +2 = 34
    // End of List
    0x00, 0x00, // +2 pad = 36
    0x3f, 0x02, 0x00, 0x00 // +4 = 40
  };
  amalgam_serializer_test<ReorderedMutableStruct, MutableStruct>(xcdr1, expected);
}

TEST(mutable_tests, to_reordered_xcdr2_test)
{
  amalgam_serializer_test<MutableStruct, ReorderedMutableStruct>(
    xcdr2, mutable_struct_expected_xcdr2);
}

TEST(mutable_tests, from_reordered_xcdr2_test)
{
  const unsigned char expected[] = {
    // Delimiter
    0x00, 0x00, 0x00, 0x22, // +4 = 4
    // long_field
    0x00, 0x00, 0x00, 0x06, // PID +4 = 8
    0x7f, 0xff, 0xff, 0xff, // +4 = 12
    // long_long_field
    0x00, 0x00, 0x00, 0x0a, // PID +4 = 16
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 24
    // octet_field
    0x00, 0x00, 0x00, 0x08, // PID +4 = 28
    0x01, // +1 = 29
    // short_field
    0x00, 0x00, 0x00, // +3 pad = 32
    0x00, 0x00, 0x00, 0x04, // PID +4 = 36
    0x7f, 0xff // +2 = 38
  };
  amalgam_serializer_test<ReorderedMutableStruct, MutableStruct>(xcdr2, expected);
}

// Additional Field Tests ----------------------------------------------------
// Test compatibility between two structures with different fields

TEST(mutable_tests, to_additional_field_xcdr1_test)
{
  MutableStruct value;
  set_values(value);
  AdditionalFieldMutableStruct result;
  amalgam_serializer_test(xcdr1, mutable_struct_expected_xcdr1, value, result);
  /// TODO(iguessthidlldo): Test for correct try construct behavior (default
  /// value?) if we decide on that.
}

TEST(mutable_tests, from_additional_field_xcdr1_test)
{
  const unsigned char expected[] = {
    // long_field
    0xc0, 0x06, 0x00, 0x04, // PID +4 = 4
    0x7f, 0xff, 0xff, 0xff, // +4 = 8
    // additional_field
    0xc0, 0x01, 0x00, 0x04, // PID +4 = 12
    0x12, 0x34, 0x56, 0x78, // +4 = 16
    // long_long_field
    0xc0, 0x0a, 0x00, 0x08, // PID +4 = 20
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 28
    // octet_field
    0xc0, 0x08, 0x00, 0x01, // PID +4 = 32
    0x01, // +1 = 33
    // short_field
    0x00, 0x00, 0x00, // +3 pad = 36
    0xc0, 0x04, 0x00, 0x02, // PID +4 = 40
    0x7f, 0xff, // +2 = 42
    // End of List
    0x00, 0x00, // +2 pad = 44
    0x3f, 0x02, 0x00, 0x00 // +4 = 48
  };
  amalgam_serializer_test<AdditionalFieldMutableStruct, MutableStruct>(xcdr1, expected);
}

TEST(mutable_tests, to_additional_field_xcdr2_test)
{
  MutableStruct value;
  set_values(value);
  AdditionalFieldMutableStruct result;
  amalgam_serializer_test(xcdr2, mutable_struct_expected_xcdr2, value, result);
  /// TODO(iguessthidlldo): Test for correct try construct behavior (default
  /// value?) if we decide on that.
}

TEST(mutable_tests, from_additional_field_must_understand_test)
{
  const unsigned char additional_field_must_understand[] = {
    // Delimiter
    0x00, 0x00, 0x00, 0x2a, // +4 = 4
    // long_field
    0x00, 0x00, 0x00, 0x06, // PID +4 = 8
    0x7f, 0xff, 0xff, 0xff, // +4 = 12
    // additional_field @must_understand
    0x80, 0x00, 0x00, 0x01, // PID +4 = 16
    0x12, 0x34, 0x56, 0x78, // +4 = 20
    // long_long_field
    0x00, 0x00, 0x00, 0x0a, // PID +4 = 24
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 32
    // octet_field
    0x00, 0x00, 0x00, 0x08, // PID +4 = 36
    0x01, // +1 = 37
    // short_field
    0x00, 0x00, 0x00, // +3 pad = 40
    0x00, 0x00, 0x00, 0x04, // PID +4 = 44
    0x7f, 0xff // +2 = 46
  };

  // Deserialization should fail for unknown must_understand field
  {
    ACE_Message_Block buffer(1024);
    buffer.copy((const char*)additional_field_must_understand, sizeof(additional_field_must_understand));
    Serializer serializer(&buffer, xcdr2);

    MutableStruct result;
    EXPECT_FALSE(serializer >> result);
  }

  // Deserialize and Compare C++ Values
  {
    ACE_Message_Block buffer(1024);
    buffer.copy((const char*)additional_field_must_understand, sizeof(additional_field_must_understand));
    Serializer serializer(&buffer, xcdr2);

    AdditionalFieldMutableStruct result;
    ASSERT_TRUE(serializer >> result);

    AdditionalFieldMutableStruct value;
    set_values(value);
    EXPECT_PRED_FORMAT2(assert_values, value, result);
  }
}

TEST(mutable_tests, from_additional_field_xcdr2_test)
{
  const unsigned char expected[] = {
    // Delimiter
    0x00, 0x00, 0x00, 0x2a, // +4 = 4
    // long_field
    0x00, 0x00, 0x00, 0x06, // PID +4 = 8
    0x7f, 0xff, 0xff, 0xff, // +4 = 12
    // additional_field
    0x00, 0x00, 0x00, 0x01, // PID +4 = 16
    0x12, 0x34, 0x56, 0x78, // +4 = 20
    // long_long_field
    0x00, 0x00, 0x00, 0x0a, // PID +4 = 24
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // +8 = 32
    // octet_field
    0x00, 0x00, 0x00, 0x08, // PID +4 = 36
    0x01, // +1 = 37
    // short_field
    0x00, 0x00, 0x00, // +3 pad = 40
    0x00, 0x00, 0x00, 0x04, // PID +4 = 44
    0x7f, 0xff // +2 = 46
  };
  amalgam_serializer_test<AdditionalFieldMutableStruct, MutableStruct>(xcdr2, expected);
}
*/
TEST(mutable_tests, length_code_test)
{
  const unsigned char expected[] = {
    0x00, 0x00, 0x00, 0x83, // +4 = 4  Delimiter (all 10 fields)
// 0
    0x00, 0x00, 0x00, 0x00, // +4 = 8  PID
    0x01, // +1 = 9  octet_field
    0x00, 0x00, 0x00, // +3 = 12  pad
// 1
    0x10, 0x00, 0x00, 0x01, // +4 = 16  PID
    0x01, 0x02, // +2 = 18  short_field
    0x00, 0x00, // +2 = 20  pad
// 2
    0x20, 0x00, 0x00, 0x02, // +4 = 24  PID
    0x01, 0x02, 0x03, 0x04, // +4 = 28  long_field
// 3
    0x30, 0x00, 0x00, 0x03, // +4 = 32  PID
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, // +8 = 40  long_long_field
// 4
    0x40, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05, // +8 = 48  PID
    0x01, 0x02, 0x03, 0x04, 0x05, // +5 = 53  octet5_field
    0x00, 0x00, 0x00, // +3 = 56  pad
// 5
    0x50, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06, // +8 = 64  PID
    0x00, 0x01, 0x00, 0x02, 0x00, 0x03, // +6 = 70  short3_field
    0x00, 0x00, // +2 = 72  pad
// 6
    0x60, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, // +8 = 80  PID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03, // +20 = 100
// 7
    0x70, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x03, // +8 = 108  PID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, // +24 = 132

    0x40, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x03, // +8 = 140  PID
    0x01, 0x00, 0x01, // +3 = 143  boolean3_field
    0x00, // +1 = 144  pad

    0x40, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x07, // +8 = 152  PID
    0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x04 // +7 = 159  Struct7

  };
  LengthCodeStruct value = { //LC  size
    0x01,                    // 0    1
    0x0102,                  // 1    2
    0x01020304,              // 2    4
    0x0102030405060708,      // 3    8
    {1,2,3,4,5},             // 4    5
    {1,2,3},                 // 5    6
    {{1,2},3},               // 6   20
    {{{1,2},3},4},           // 7   24
    {true,false,true},       // 4    3
    {{1,2,3},4}              // 4    7

  };

  LengthCodeStruct result;
  amalgam_serializer_test<LengthCodeStruct, LengthCodeStruct>(xcdr2, expected, value, result);
}

int main(int argc, char* argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
