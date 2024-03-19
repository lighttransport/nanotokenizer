#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <functional>
#include <unordered_set>

namespace nanotrie {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat"
#pragma clang diagnostic ignored "-Wpadded"
#endif

// FNV1-a hash
inline uint32_t FNV1a(const uint8_t *data, const size_t nbytes) {
  constexpr uint32_t kOffsetBasis = 0x811c9dc5;
  constexpr uint32_t kPrime = 0x01000193;

  uint32_t hash = kOffsetBasis;
  for (size_t i = 0; i < nbytes; i++) {
    hash = (hash ^ data[i]) * kPrime;
  }

  return hash;
}

// Simple single-token and fixed-size hashmap
// sizeof(KeyType) must be <= sizeof(uint32_t)
// Up to 2G items.
template<typename KeyType, typename ValueType, size_t N>
struct TokenHashMap
{
  struct Bucket {
    uint32_t  count{0};  // # of entities.
    uint32_t  offset{0}; // offset index to buffer.
  };

  struct Entity {
    KeyType key{};  // NOTE: padded when sizeof(KeyType) <= 4.
    ValueType value{};
  };

  //
  // Add key/value to the map.
  // When key already exists, overwrite its value.
  //
  bool update(const KeyType key, const ValueType value) {

    const uint32_t hash = FNV1a(reinterpret_cast<const uint8_t *>(&key), sizeof(KeyType));
    uint32_t idx = hash % N;

    if (!buckets[idx].count) {
      if (buffer.size() >= (std::numeric_limits<int32_t>::max)()) {
        // Too many items
        return false;
      }

      // newly add entry to buffer.
      Entity e;
      e.key = key;
      e.value = value;

      buckets[idx].count = 1;
      buckets[idx].offset = uint32_t(buffer.size());

      buffer.emplace_back(std::move(e));
    } else {

      uint32_t offset = buckets[idx].offset;

      // Check if key already exists.
      // TODO: Use binary search
      for (size_t i = 0; i < buckets[idx].count; i++) {
        if (buffer[offset + i].key == key) {
          // replace
          buffer[offset + i].value = value;
          return true;
        }
      }

      if (buffer.size() >= (std::numeric_limits<int32_t>::max)()) {
        // Too many items
        return false;
      }

      // insert Entity to `offset`
      Entity e;
      e.key = key;
      e.value = value;

      buffer.insert(buffer.begin() + offset + buckets[idx].count, e);

      buckets[idx].count++;

      // sort item.
      std::sort(buffer.begin() + offset, buffer.begin() + offset + buckets[idx].count, [](const Entity &a, const Entity &b) {
        return std::less<KeyType>()(a.key, b.key);
      });

      // Adjust `offset` in other buckets.
      for (size_t i = 0; i < N; i++) {
        if (buckets[i].offset > offset) {
          buckets[i].offset++;
        }
      }

    }

    return true;
  }

  bool count(const KeyType key) {
    const uint32_t hash = FNV1a(reinterpret_cast<const uint8_t *>(&key), sizeof(KeyType));
    uint32_t idx = hash % N;

    if (!buckets[idx].count) {
      return false;
    }

    uint32_t buffer_offset = buckets[idx].offset;

    if (buckets[idx].count <= 4) {
      // Use linear scan.
      for (size_t i = 0; i < buckets[idx].count; i++) {
        if (buffer[buffer_offset + i].key == key) {
          return true;
        }
      }
    } else {
      // Use binary search. Keys should be sorted.
      uint32_t left_idx = 0;
      uint32_t right_idx = buckets[idx].count - 1; // inclusive

      while (left_idx <= right_idx) {
        uint32_t middle_idx = left_idx + (right_idx - left_idx) / 2;
        if (buffer[buffer_offset + middle_idx].key == key) {
          return true;
        }

        if (buffer[buffer_offset + middle_idx].key < key) {
          left_idx = middle_idx + 1;
        } else {
          right_idx = middle_idx - 1;
        }
      }
    }

    return false;
  }

  bool find(const KeyType key, ValueType &result) {
    const uint32_t hash = FNV1a(reinterpret_cast<const uint8_t *>(&key), sizeof(KeyType));
    uint32_t idx = hash % N;

    if (!buckets[idx].count) {
      return false;
    }

    uint32_t buffer_offset = buckets[idx].offset;

    if (buckets[idx].count <= 4) {
      // Use linear scan.
      for (size_t i = 0; i < buckets[idx].count; i++) {
        if (buffer[buffer_offset + i].key == key) {
          result = buffer[buffer_offset + i].value;
          return true;
        }
      }
    } else {
      // Use binary search. Keys should be sorted.
      uint32_t left_idx = 0;
      uint32_t right_idx = buckets[idx].count - 1; // inclusive

      while (left_idx <= right_idx) {
        uint32_t middle_idx = left_idx + (right_idx - left_idx) / 2;
        if (buffer[buffer_offset + middle_idx].key == key) {
          result = buffer[buffer_offset + middle_idx].value;
          return true;
        }

        if (buffer[buffer_offset + middle_idx].key < key) {
          left_idx = middle_idx + 1;
        } else {
          right_idx = middle_idx - 1;
        }
      }
    }

    return false;
  }

  bool erase(const KeyType key) {

    const uint32_t hash = FNV1a(reinterpret_cast<const uint8_t *>(&key), sizeof(KeyType));
    uint32_t idx = hash % N;

    if (!buckets[idx].count) {
      return false;
    }

    uint32_t buffer_offset = buckets[idx].offset;

    if (buckets[idx].count <= 4) {
      // Use linear scan.
      for (size_t i = 0; i < buckets[idx].count; i++) {
        if (buffer[buffer_offset + i].key == key) {

          buffer.erase(buffer.begin() + buffer_offset + i);
          buckets[idx].count--;

          // Adjust `offset` in other buckets.
          for (size_t k = 0; k < N; k++) {
            if (buckets[k].offset > buffer_offset) {
              buckets[k].offset--;
            }
          }

          return true;
        }
      }
    } else {
      // Use binary search. Keys should be sorted.
      uint32_t left_idx = 0;
      uint32_t right_idx = buckets[idx].count - 1; // inclusive

      while (left_idx <= right_idx) {
        uint32_t middle_idx = left_idx + (right_idx - left_idx) / 2;
        if (buffer[buffer_offset + middle_idx].key == key) {

          buffer.erase(buffer.begin() + buffer_offset + middle_idx);
          buckets[idx].count--;

          // Adjust `offset` in other buckets.
          for (size_t i = 0; i < N; i++) {
            if (buckets[i].offset > buffer_offset) {
              buckets[i].offset--;
            }
          }

          return true;
        }

        if (buffer[buffer_offset + middle_idx].key < key) {
          left_idx = middle_idx + 1;
        } else {
          right_idx = middle_idx - 1;
        }
      }
    }

    return false;
  }

  bool deserialize(const Bucket *in_buckets, size_t nbuckets, const Entity *in_entities, size_t nentities) {

    if (!in_buckets || !in_entities) {
      return false;
    }

    if (nbuckets != N) {
      return false;
    }

    if (nentities == 0) {
      return false;
    }

    memcpy(&buckets[0], in_buckets, sizeof(Bucket) * N);
    // validate offset and count
    std::unordered_set<uint32_t> counts; // for overlap test.
    for (size_t i = 0; i < N; i++) {
      for (size_t k = 0; k < buckets[i].count; k++) {
        if (buckets[i].offset + k >= nentities) {
          return false;
        }

        if (counts.count(buckets[i].offset + k)) {
          return false;
        }
        counts.insert(buckets[i].offset + k);
      }
    }

    if (counts.size() != nentities) {
      return false;
    }

    buffer.resize(nentities);
    memcpy(buffer.data(), in_entities, sizeof(Entity) * nentities);

    // TODO: validate key in bufffer.
  }

  bool serialize(std::vector<uint8_t> &dst) {
    size_t bucket_size = sizeof(Bucket) * N;
    size_t buffer_size = sizeof(Entity) * buffer.size();

    dst.resize(bucket_size + buffer_size);

    memcpy(dst.data(), &buckets[0], bucket_size);
    memcpy(dst.data() + sizeof(Bucket) * N, buffer.data(), buffer_size);

    return true;
  }

  std::array<Bucket, N> buckets{}; // init with zeros
  std::vector<Entity> buffer;
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}
