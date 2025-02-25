#include "U8.hpp"
#include <core/common.h>
#include <rsl/SimpleReader.hpp>

namespace librii::U8 {

struct rvlArchiveHeader {
  rsl::bu32 magic; // 00
  struct {
    rsl::bs32 offset; // 04
    rsl::bs32 size;   // 08
  } nodes;
  struct {
    rsl::bs32 offset; // 0C
  } files;

  std::array<u8, 16> watermark;
};

static_assert(sizeof(rvlArchiveHeader) == 32);

struct rvlArchiveNode {
  rsl::bu32 packed_type_name;
  union {
    struct {
      rsl::bu32 offset;
      rsl::bu32 size;
    } file;
    struct {
      rsl::bu32 parent;
      rsl::bu32 sibling_next;
    } folder;
  };
};

static_assert(sizeof(rvlArchiveNode) == 12);

struct LowU8Archive {
  rvlArchiveHeader header;
  std::vector<rvlArchiveNode> nodes;
  std::string strings;       // One giant string
  std::vector<u8> file_data; // One giant buffer
};

#define rvlArchiveNodeIsFolder(node) ((node).packed_type_name & 0xff000000)
#define rvlArchiveNodeGetName(node) ((node).packed_type_name & 0x00ffffff)

static inline const rvlArchiveNode*
rvlArchiveHeaderGetNodes(const rvlArchiveHeader* self) {
  assert(self->nodes.offset >= sizeof(rvlArchiveHeader));
  return (const rvlArchiveNode*)(((u8*)self) + self->nodes.offset);
}
static inline const u8*
rvlArchiveHeaderGetFileData(const rvlArchiveHeader* self) {
  assert(self->files.offset >= sizeof(rvlArchiveHeader));
  return (const u8*)((u8*)self + self->files.offset);
}

template <typename T> bool SafeMemCopy(T& dest, std::span<const u8> data) {
  assert(data.size_bytes() >= sizeof(T));
  if (data.size_bytes() < sizeof(T)) {
    std::fill((u8*)&dest, (u8*)(&dest + 1), 0);
    return false;
  }

  std::memcpy(&dest, data.data(), sizeof(T));
  return true;
}

bool RangeContains(std::span<const u8> range, const void* ptr) {
  return ptr >= range.data() && ptr < (range.data() + range.size());
}
bool RangeContainsInclusive(std::span<const u8> range, const void* ptr) {
  return ptr >= range.data() && ptr <= range.data() + range.size();
}

bool LoadU8Archive(LowU8Archive& result, std::span<const u8> data) {
  if (!SafeMemCopy(result.header, data))
    return false;

  const auto* nodes = rvlArchiveHeaderGetNodes(
      reinterpret_cast<const rvlArchiveHeader*>(data.data()));
  if (!RangeContains(data, nodes))
    return false;

  const auto node_count = nodes[0].folder.sibling_next;
  if (!RangeContainsInclusive(data, nodes + node_count))
    return false;

  result.nodes = {nodes, nodes + node_count};

  const char* strings = reinterpret_cast<const char*>(nodes + node_count);
  if (!RangeContains(data, strings))
    return false;

  const char* strings_end =
      reinterpret_cast<const char*>(nodes) + result.header.nodes.size;
  if (!RangeContainsInclusive(data, strings_end))
    return false;

  result.strings = {strings, strings_end};

  auto* fd_begin = rvlArchiveHeaderGetFileData(
      reinterpret_cast<const rvlArchiveHeader*>(data.data()));
  if (!RangeContains(data, fd_begin))
    return false;

  // For some reason the FD pointer is actually just the start of the file
  fd_begin = data.data();

  result.file_data = {fd_begin, data.data() + data.size()};

  return true;
}

bool LoadU8Archive(U8Archive& result, std::span<const u8> data) {
  LowU8Archive low;
  if (!LoadU8Archive(low, data))
    return false;

  result.watermark = low.header.watermark;
  for (auto& node : low.nodes) {
    U8Archive::Node tmp = {.is_folder = (bool)rvlArchiveNodeIsFolder(node),
                           .name = low.strings.data() +
                                   rvlArchiveNodeGetName(node)};
    if (tmp.is_folder) {
      tmp.folder.parent = node.folder.parent;
      tmp.folder.sibling_next = node.folder.sibling_next;
    } else {
      tmp.file.offset = node.file.offset;
      tmp.file.size = node.file.size;
    }

    result.nodes.push_back(tmp);
  }

  result.file_data = std::move(low.file_data);
  return true;
}

} // namespace librii::U8
