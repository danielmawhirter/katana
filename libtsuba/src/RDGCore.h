#ifndef KATANA_LIBTSUBA_RDGCORE_H_
#define KATANA_LIBTSUBA_RDGCORE_H_

#include <memory>

#include <arrow/api.h>

#include "RDGPartHeader.h"
#include "katana/config.h"
#include "tsuba/FileView.h"

namespace tsuba {

class KATANA_EXPORT RDGCore {
public:
  RDGCore() { InitEmptyTables(); }

  RDGCore(RDGPartHeader&& part_header) : part_header_(std::move(part_header)) {
    InitEmptyTables();
  }

  bool Equals(const RDGCore& other) const;

  katana::Result<void> AddNodeProperties(
      const std::shared_ptr<arrow::Table>& table);

  katana::Result<void> AddEdgeProperties(
      const std::shared_ptr<arrow::Table>& table);

  katana::Result<void> RemoveNodeProperty(uint32_t i);

  katana::Result<void> RemoveEdgeProperty(uint32_t i);

  //
  // Accessors and Mutators
  //

  const std::shared_ptr<arrow::Table>& node_table() const {
    return node_table_;
  }
  void set_node_table(std::shared_ptr<arrow::Table>&& node_table) {
    node_table_ = std::move(node_table);
  }

  const std::shared_ptr<arrow::Table>& edge_table() const {
    return edge_table_;
  }
  void set_edge_table(std::shared_ptr<arrow::Table>&& edge_table) {
    edge_table_ = std::move(edge_table);
  }

  const FileView& topology_file_storage() const {
    return topology_file_storage_;
  }
  FileView& topology_file_storage() { return topology_file_storage_; }
  void set_topology_file_storage(FileView&& topology_file_storage) {
    topology_file_storage_ = std::move(topology_file_storage);
  }

  const RDGPartHeader& part_header() const { return part_header_; }
  RDGPartHeader& part_header() { return part_header_; }
  void set_part_header(RDGPartHeader&& part_header) {
    part_header_ = std::move(part_header);
  }

  katana::Result<void> RegisterTopologyFile(const std::string& new_top) {
    part_header_.set_topology_path(new_top);
    return topology_file_storage_.Unbind();
  }

private:
  void InitEmptyTables();

  //
  // Data
  //

  std::shared_ptr<arrow::Table> node_table_;
  std::shared_ptr<arrow::Table> edge_table_;

  FileView topology_file_storage_;

  RDGPartHeader part_header_;
};

}  // namespace tsuba

#endif
