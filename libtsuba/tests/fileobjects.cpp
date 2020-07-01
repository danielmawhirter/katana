#include "galois/Logging.h"
#include "galois/FileSystem.h"
#include "tsuba/FileFrame.h"
#include "tsuba/FileView.h"
#include "tsuba/file.h"
#include "tsuba/tsuba.h"

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

#define EXP_WRITE_COUNT 15
#define READ_PARTIAL 4567

static void fill_bits(uint8_t bits[], int n) {
  for (int i = 0; i < n; ++i) {
    bits[i] = std::rand();
  }
}

static void exponential(uint8_t bits[], std::string& dir) {
  // Write
  std::string filename = dir + "exponential";
  auto ff              = tsuba::FileFrame();
  int err              = ff.Init();
  GALOIS_LOG_ASSERT(!err);

  uint8_t* ptr     = bits;
  uint64_t running = 0;
  for (int i = 0; i < EXP_WRITE_COUNT; ++i) {
    arrow::Status aro_sts = ff.Write(ptr, 1 << i);
    GALOIS_LOG_ASSERT(aro_sts.ok());
    ptr += 1 << i;
    running += 1 << i;
  }
  ff.Bind(filename);
  err = ff.Persist();
  GALOIS_LOG_ASSERT(!err);

  // Validate
  tsuba::StatBuf buf;
  err = tsuba::FileStat(filename, &buf);
  GALOIS_LOG_ASSERT(!err);
  GALOIS_LOG_ASSERT(buf.size == running);

  // Read
  auto fv = tsuba::FileView();
  err     = fv.Bind(filename);
  GALOIS_LOG_ASSERT(!err);
  auto aro_res = fv.Read(running);
  GALOIS_LOG_ASSERT(aro_res.ok());
  auto aro_buf = aro_res.ValueOrDie();
  GALOIS_LOG_ASSERT(static_cast<uint64_t>(aro_buf->size()) == running);
  GALOIS_LOG_ASSERT(!memcmp(aro_buf->data(), bits, running));
}

static void the_big_one(uint8_t bits[], uint64_t num_bytes, std::string& dir) {
  // Write
  std::string filename = dir + "the-big-one";
  auto ff              = tsuba::FileFrame();
  int err              = ff.Init();
  GALOIS_LOG_ASSERT(!err);

  arrow::Status aro_sts = ff.Write(bits, num_bytes);
  GALOIS_LOG_ASSERT(aro_sts.ok());
  ff.Bind(filename);
  err = ff.Persist();
  GALOIS_LOG_ASSERT(!err);

  // Validate
  tsuba::StatBuf buf;
  err = tsuba::FileStat(filename, &buf);
  GALOIS_LOG_ASSERT(!err);
  GALOIS_LOG_ASSERT(buf.size == num_bytes);

  // Read
  uint64_t res[num_bytes];
  auto fv = tsuba::FileView();
  err     = fv.Bind(filename);
  GALOIS_LOG_ASSERT(!err);
  arrow::Result<int64_t> aro_res = fv.Read(READ_PARTIAL, res);
  GALOIS_LOG_ASSERT(aro_res.ok());
  int64_t bytes_read = aro_res.ValueOrDie();
  GALOIS_LOG_ASSERT(bytes_read == READ_PARTIAL);
  GALOIS_LOG_ASSERT(!memcmp(res, bits, READ_PARTIAL));
}

static void silly(uint8_t bits[], uint64_t num_bytes, std::string& dir) {
  // Write
  std::string filename = dir + "silly";
  auto ff              = tsuba::FileFrame();
  int err              = ff.Init(num_bytes * 2);
  GALOIS_LOG_ASSERT(!err);

  err = ff.Persist();
  GALOIS_LOG_ASSERT(err);

  auto aro_buf          = std::make_shared<arrow::Buffer>(bits, num_bytes);
  arrow::Status aro_sts = ff.Write(aro_buf);
  GALOIS_LOG_ASSERT(aro_sts.ok());
  err = ff.Persist();
  GALOIS_LOG_ASSERT(err);
  ff.Bind(filename);
  err = ff.Persist();
  GALOIS_LOG_ASSERT(!err);

  // Validate
  tsuba::StatBuf buf;
  err = tsuba::FileStat(filename, &buf);
  GALOIS_LOG_ASSERT(!err);
  GALOIS_LOG_ASSERT(buf.size == num_bytes);

  // Read
  auto fv = tsuba::FileView();
  err     = fv.Bind(filename + "not-a-file");
  GALOIS_LOG_ASSERT(err);

  err = fv.Bind(filename);
  GALOIS_LOG_ASSERT(!err);

  aro_sts = fv.Seek(num_bytes - READ_PARTIAL);
  GALOIS_LOG_ASSERT(aro_sts.ok());
  arrow::Result<long int> aro_res = fv.Tell();
  GALOIS_LOG_ASSERT(aro_res.ok());
  GALOIS_LOG_ASSERT(static_cast<uint64_t>(aro_res.ValueOrDie()) ==
                    num_bytes - READ_PARTIAL);

  arrow::Result<std::shared_ptr<arrow::Buffer>> aro_rest = fv.Read(num_bytes);
  GALOIS_LOG_ASSERT(aro_rest.ok());
  auto aro_buff = aro_rest.ValueOrDie();
  GALOIS_LOG_ASSERT(static_cast<uint64_t>(aro_buff->size()) == READ_PARTIAL);
  GALOIS_LOG_ASSERT(
      !memcmp(aro_buff->data(), &bits[num_bytes - READ_PARTIAL], READ_PARTIAL));

  aro_sts = fv.Close();
  GALOIS_LOG_ASSERT(aro_sts.ok());
  GALOIS_LOG_ASSERT(fv.closed());
  aro_sts = ff.Close();
  GALOIS_LOG_ASSERT(aro_sts.ok());
  GALOIS_LOG_ASSERT(ff.closed());
}

int main() {
  uint64_t num_bytes = 1 << EXP_WRITE_COUNT;
  uint8_t bits[num_bytes];
  fill_bits(bits, num_bytes);

  auto unique_result = galois::CreateUniqueDirectory("/tmp/fileobjects-");
  GALOIS_LOG_ASSERT(unique_result);
  std::string temp_dir(std::move(unique_result.value()));

  exponential(bits, temp_dir);
  the_big_one(bits, num_bytes, temp_dir);
  silly(bits, num_bytes, temp_dir);

  fs::remove_all(temp_dir);
  return 0;
}
