import struct
import unittest
from prepare_data import validate_index


class PackageIndexTests(unittest.TestCase):
    def test_realistic_index_with_empty_entry(self):
        self.assertEqual(validate_index(struct.pack("<5I", 3, 0, 12, 12, 29), 29), 3)

    def test_truncated_package_rejected(self):
        with self.assertRaisesRegex(ValueError, "package size"):
            validate_index(struct.pack("<4I", 2, 0, 12, 29), 28)

    def test_wrong_pair_rejected(self):
        with self.assertRaisesRegex(ValueError, "package size"):
            validate_index(struct.pack("<4I", 2, 1, 12, 29), 29)

    def test_corrupted_offsets_rejected(self):
        with self.assertRaisesRegex(ValueError, "monotonic"):
            validate_index(struct.pack("<5I", 3, 0, 20, 12, 29), 29)

    def test_corrupted_entry_count_rejected(self):
        with self.assertRaisesRegex(ValueError, "entry count"):
            validate_index(struct.pack("<4I", 5, 0, 12, 29), 29)

    def test_truncated_index_rejected(self):
        with self.assertRaisesRegex(ValueError, "length"):
            validate_index(b"\0" * 13, 0)


if __name__ == "__main__":
    unittest.main()
