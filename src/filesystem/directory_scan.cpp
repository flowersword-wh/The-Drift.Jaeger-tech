#include <drift/filesystem/directory_scan.h>
#include <drift/filesystem/file_hash.h>

namespace fs = std::filesystem;

drift::filesystem::DirectoryEntries
drift::filesystem::scan_directory(const fs::path &root)
{
	drift::filesystem::DirectoryEntries entries;

	for (const auto &entry : fs::recursive_directory_iterator(root)) {
		drift::filesystem::EntryData data{};

		data.relative_path = entry.path().lexically_relative(root);

		if (entry.is_directory()) {
			data.type = drift::filesystem::EntryType::Directory;
		} else if (entry.is_regular_file()) {
			data.type = drift::filesystem::EntryType::File;
			data.file_size = entry.file_size();

			data.hash = calculate_hash(entry.path());
		} else {
			continue;
		}

		entries.emplace(data.relative_path.generic_string(), std::move(data));
	}

	return entries;
}
