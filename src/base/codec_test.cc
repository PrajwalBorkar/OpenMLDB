/*
 * codec_test.cc
 */


#include "base/codec.h"
#include "storage/segment.h"
#include "gtest/gtest.h"
#include "proto/common.pb.h"
#include "proto/tablet.pb.h"
#include "base/kv_iterator.h"
#include <string>
#include <vector>


namespace rtidb {
namespace base {

class CodecTest : public ::testing::Test {

public:
    CodecTest(){}
    ~CodecTest() {}
};

TEST_F(CodecTest, EncodeRows_empty) {
    std::vector<std::pair<uint64_t, ::rtidb::base::Slice>> data;
    std::string pairs;
    int32_t size = ::rtidb::base::EncodeRows(data, 0, &pairs);
    ASSERT_EQ(size, 0);
}


TEST_F(CodecTest, EncodeRows_invalid) {
    std::vector<std::pair<uint64_t, ::rtidb::base::Slice>> data;
    int32_t size = ::rtidb::base::EncodeRows(data, 0, NULL);
    ASSERT_EQ(size, -1);
}

TEST_F(CodecTest, EncodeRows) {
    std::vector<std::pair<uint64_t, ::rtidb::base::Slice>> data;
    std::string test1 = "value1";
    std::string test2 = "value2";
    std::string empty;
    uint32_t total_block_size = test1.length() + test2.length() + empty.length();
    data.push_back(std::make_pair(1, ::rtidb::base::Slice(test1.c_str(), test1.length())));
    data.push_back(std::make_pair(2, ::rtidb::base::Slice(test2.c_str(), test2.length())));
    data.push_back(std::make_pair(3, ::rtidb::base::Slice(empty.c_str(), empty.length())));
    std::string pairs;
    int32_t size = ::rtidb::base::EncodeRows(data, total_block_size, &pairs);
    ASSERT_EQ(size, 3 * 12 + 6 + 6);
    std::vector<std::pair<uint64_t, std::string*>> new_data;
    ::rtidb::base::Decode(&pairs, new_data);
    ASSERT_EQ(data.size(), new_data.size());
    ASSERT_EQ(new_data[0].second->compare(test1), 0);
    ASSERT_EQ(new_data[1].second->compare(test2), 0);
    ASSERT_EQ(new_data[2].second->compare(empty), 0);
}

TEST_F(CodecTest, NULLTest) {
    Schema schema;
    ::rtidb::common::ColumnDesc* col = schema.Add();
    col->set_name("col1");
    col->set_data_type(::rtidb::type::kSmallInt);
    col = schema.Add();
    col->set_name("col2");
    col->set_data_type(::rtidb::type::kBool);
    col = schema.Add();
    col->set_name("col3");
    col->set_data_type(::rtidb::type::kVarchar);
    RowBuilder builder(schema);
    uint32_t size = builder.CalTotalLength(1);
    std::string row;
    row.resize(size);
    builder.SetBuffer(reinterpret_cast<int8_t*>(&(row[0])), size);
    std::string st("1");
    ASSERT_TRUE(builder.AppendNULL());
    ASSERT_TRUE(builder.AppendBool(false));
    ASSERT_TRUE(builder.AppendString(st.c_str(), 1));
    RowView view(schema, reinterpret_cast<int8_t*>(&(row[0])), size);
    ASSERT_TRUE(view.IsNULL(0));
    char* ch = NULL;
    uint32_t length = 0;
    bool val1 = true;
    ASSERT_EQ(view.GetBool(1, &val1), 0);
    ASSERT_FALSE(val1);
    ASSERT_EQ(view.GetString(2, &ch, &length), 0);
}

TEST_F(CodecTest, Normal) {
    Schema schema;
    ::rtidb::common::ColumnDesc* col = schema.Add();
    col->set_name("col1");
    col->set_data_type(::rtidb::type::kInt);
    col = schema.Add();
    col->set_name("col2");
    col->set_data_type(::rtidb::type::kSmallInt);
    col = schema.Add();
    col->set_name("col3");
    col->set_data_type(::rtidb::type::kFloat);
    col = schema.Add();
    col->set_name("col4");
    col->set_data_type(::rtidb::type::kDouble);
    col = schema.Add();
    col->set_name("col5");
    col->set_data_type(::rtidb::type::kBigInt);
    RowBuilder builder(schema);
    uint32_t size = builder.CalTotalLength(0);
    std::string row;
    row.resize(size);
    builder.SetBuffer(reinterpret_cast<int8_t*>(&(row[0])), size);
    ASSERT_TRUE(builder.AppendInt32(1));
    ASSERT_TRUE(builder.AppendInt16(2));
    ASSERT_TRUE(builder.AppendFloat(3.1));
    ASSERT_TRUE(builder.AppendDouble(4.1));
    ASSERT_TRUE(builder.AppendInt64(5));
    RowView view(schema, reinterpret_cast<int8_t*>(&(row[0])), size);
    int32_t val = 0;
    ASSERT_EQ(view.GetInt32(0, &val), 0);
    ASSERT_EQ(val, 1);
    int16_t val1 = 0;
    ASSERT_EQ(view.GetInt16(1, &val1), 0);
    ASSERT_EQ(val1, 2);
}

TEST_F(CodecTest, Encode) {
    Schema schema;
    for (int i = 0; i < 10; i++) {
        ::rtidb::common::ColumnDesc* col = schema.Add();
        col->set_name("col" + std::to_string(i));
        if (i % 3 == 0) {
            col->set_data_type(::rtidb::type::kSmallInt);
        } else if (i % 3 == 1) {
            col->set_data_type(::rtidb::type::kDouble);
        } else {
            col->set_data_type(::rtidb::type::kVarchar);
        }
    }
    RowBuilder builder(schema);
    uint32_t size = builder.CalTotalLength(30);
    std::string row;
    row.resize(size);
    builder.SetBuffer(reinterpret_cast<int8_t*>(&(row[0])), size);
    for (int i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            ASSERT_TRUE(builder.AppendInt16(i));
        } else if (i % 3 == 1) {
            ASSERT_TRUE(builder.AppendDouble(2.3));
        } else {
            std::string str(10, 'a' + i);
            ASSERT_TRUE(builder.AppendString(str.c_str(), str.length()));
        }
    }
    ASSERT_FALSE(builder.AppendInt16(1234));
    RowView view(schema, reinterpret_cast<int8_t*>(&(row[0])), size);
    for (int i = 0; i < 10; i++) {
        if (i % 3 == 0) {
            int16_t val = 0;
            ASSERT_EQ(view.GetInt16(i, &val), 0);
            ASSERT_EQ(val, i);
        } else if (i % 3 == 1) {
            double val = 0.0;
            ASSERT_EQ(view.GetDouble(i, &val), 0);
            ASSERT_EQ(val, 2.3);
        } else {
            char* ch = NULL;
            uint32_t length = 0;
            ASSERT_EQ(view.GetString(i, &ch, &length), 0);
            std::string str(ch, length);
            ASSERT_STREQ(str.c_str(), std::string(10, 'a' + i).c_str());
        }
    }
    int16_t val = 0;
    ASSERT_EQ(view.GetInt16(10, &val), -1);
}

TEST_F(CodecTest, AppendNULL) {
    Schema schema;
    for (int i = 0; i < 20; i++) {
        ::rtidb::common::ColumnDesc* col = schema.Add();
        col->set_name("col" + std::to_string(i));
        if (i % 3 == 0) {
            col->set_data_type(::rtidb::type::kSmallInt);
        } else if (i % 3 == 1) {
            col->set_data_type(::rtidb::type::kDouble);
        } else {
            col->set_data_type(::rtidb::type::kVarchar);
        }
    }
    RowBuilder builder(schema);
    uint32_t size = builder.CalTotalLength(30);
    std::string row;
    row.resize(size);
    builder.SetBuffer(reinterpret_cast<int8_t*>(&(row[0])), size);
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            ASSERT_TRUE(builder.AppendNULL());
            continue;
        }
        if (i % 3 == 0) {
            ASSERT_TRUE(builder.AppendInt16(i));
        } else if (i % 3 == 1) {
            ASSERT_TRUE(builder.AppendDouble(2.3));
        } else {
            std::string str(10, 'a' + i);
            ASSERT_TRUE(builder.AppendString(str.c_str(), str.length()));
        }
    }
    ASSERT_FALSE(builder.AppendInt16(1234));
    RowView view(schema, reinterpret_cast<int8_t*>(&(row[0])), size);
    for (int i = 0; i < 20; i++) {
        if (i % 3 == 0) {
            int16_t val = 0;
            int ret = view.GetInt16(i, &val);
            if (i % 2 == 0) {
                ASSERT_TRUE(view.IsNULL(i));
                ASSERT_EQ(ret, 1);
            } else {
                ASSERT_EQ(ret, 0);
                ASSERT_EQ(val, i);
            }
        } else if (i % 3 == 1) {
            double val = 0.0;
            int ret = view.GetDouble(i, &val);
            if (i % 2 == 0) {
                ASSERT_TRUE(view.IsNULL(i));
                ASSERT_EQ(ret, 1);
            } else {
                ASSERT_EQ(ret, 0);
                ASSERT_EQ(val, 2.3);
            }
        } else {
            char* ch = NULL;
            uint32_t length = 0;
            int ret = view.GetString(i, &ch, &length);
            if (i % 2 == 0) {
                ASSERT_TRUE(view.IsNULL(i));
                ASSERT_EQ(ret, 1);
            } else {
                ASSERT_EQ(ret, 0);
                std::string str(ch, length);
                ASSERT_STREQ(str.c_str(), std::string(10, 'a' + i).c_str());
            }
        }
    }
    int16_t val = 0;
    ASSERT_EQ(view.GetInt16(20, &val), -1);
}

TEST_F(CodecTest, AppendNULLAndEmpty) {
    Schema schema;
    for (int i = 0; i < 20; i++) {
        ::rtidb::common::ColumnDesc* col = schema.Add();
        col->set_name("col" + std::to_string(i));
        if (i % 2 == 0) {
            col->set_data_type(::rtidb::type::kSmallInt);
        } else {
            col->set_data_type(::rtidb::type::kVarchar);
        }
    }
    RowBuilder builder(schema);
    uint32_t size = builder.CalTotalLength(30);
    std::string row;
    row.resize(size);
    builder.SetBuffer(reinterpret_cast<int8_t*>(&(row[0])), size);
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            if (i % 3 == 0) {
                ASSERT_TRUE(builder.AppendNULL());
            } else {
                ASSERT_TRUE(builder.AppendInt16(i));
            }
        } else {
            std::string str(10, 'a' + i);
            if (i % 3 == 0) {
                ASSERT_TRUE(builder.AppendNULL());
            } else if (i % 3 == 1) {
                ASSERT_TRUE(builder.AppendString(str.c_str(), 0));
            } else {
                ASSERT_TRUE(builder.AppendString(str.c_str(), str.length()));
            }
        }
    }
    ASSERT_FALSE(builder.AppendInt16(1234));
    RowView view(schema, reinterpret_cast<int8_t*>(&(row[0])), size);
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            int16_t val = 0;
            int ret = view.GetInt16(i, &val);
            if (i % 3 == 0) {
                ASSERT_TRUE(view.IsNULL(i));
                ASSERT_EQ(ret, 1);
            } else {
                ASSERT_EQ(ret, 0);
                ASSERT_EQ(val, i);
            }
        } else {
            char* ch = NULL;
            uint32_t length = 0;
            int ret = view.GetString(i, &ch, &length);
            if (i % 3 == 0) {
                ASSERT_TRUE(view.IsNULL(i));
                ASSERT_EQ(ret, 1);
            } else if (i % 3 == 1) {
                ASSERT_EQ(ret, 0);
                ASSERT_EQ(length, 0);
            } else {
                ASSERT_EQ(ret, 0);
                std::string str(ch, length);
                ASSERT_STREQ(str.c_str(), std::string(10, 'a' + i).c_str());
            }
        }
    }
    int16_t val = 0;
    ASSERT_EQ(view.GetInt16(20, &val), -1);
}

}
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
