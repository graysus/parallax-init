
#include <string>
#include <PxResult.hpp>
#include <vector>
#include <libmount/libmount.h>

#ifndef PXMOUNT_HPP
#define PXMOUNT_HPP

namespace PxMount {
	struct FsEntry {
		std::string source = "";
		std::string target = "";
		std::string type = "";
		std::string options = "";
		int freq = 0;
		int fsck_pass = 0;
	};
	PxResult::Result<FsEntry> MakeFsEntry(struct libmnt_fs* fs);
	PxResult::Result<void> Tab_LoadFromFile(struct libmnt_table** table, std::string path);
	PxResult::Result<std::vector<FsEntry>> Tab_List(struct libmnt_table* table);
	void Tab_Destroy(struct libmnt_table* table);
	class Table {
	private:
		struct libmnt_table* table;
	public:
		Table() {

		}
		virtual ~Table() {
			Tab_Destroy(table);
		}
		inline PxResult::Result<void> LoadFromFile(std::string path) {
			return Tab_LoadFromFile(&table, path);
		}
		inline PxResult::Result<std::vector<FsEntry>> List() {
			return Tab_List(table);
		}
	};
	PxResult::Result<void> SimpleMount(std::string src, std::string dest, std::string type, unsigned long flags = 0, std::string data = "");
	PxResult::Result<void> Mount(std::string src, std::string dest = "", std::string type = "", std::string flags = "");
	PxResult::Result<void> Unmount(std::string target);
}

#endif
