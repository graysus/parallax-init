
#include "PxDefer.hpp"
#include <bits/types/error_t.h>
#include <cerrno>
#include <string>
#include <unistd.h>
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <sys/mount.h>
#include <vector>
#include <libmount/libmount.h>
#include <errno.h>
#include <PxMount.hpp>

namespace PxMount {
	PxResult::Result<FsEntry> MakeFsEntry(struct libmnt_fs* fs) {
		if (fs == NULL) {
			return PxResult::Result<FsEntry>("PxMount::MakeFsEntry (null argument)", EINVAL);
		}
		FsEntry result;
		result.source = (std::string)mnt_fs_get_source(fs);
		result.target = (std::string)mnt_fs_get_target(fs);
		result.type = (std::string)mnt_fs_get_fstype(fs);
		result.options = (std::string)mnt_fs_get_options(fs);
		result.freq = mnt_fs_get_freq(fs);
		result.fsck_pass = mnt_fs_get_passno(fs);
		return result;
	}
	void Tab_Destroy(struct libmnt_table* table) {
		if (table != NULL) {
			mnt_free_table(table);
		}
	}
	PxResult::Result<void> Tab_LoadFromFile(struct libmnt_table** table, std::string path) {
		if (table == NULL) {
			return PxResult::Result<void>("PxMount::Table::LoadFromFile", EINVAL);
		}
		*table = mnt_new_table_from_file(path.c_str());
		if (*table == NULL) {
			return PxResult::Result<void>("PxMount::Table::LoadFromFile / mnt_new_table_from_file", errno);
		}
		return PxResult::Null;
	}
	PxResult::Result<std::vector<FsEntry>> Tab_List(struct libmnt_table* table) {
		libmnt_iter* iter = mnt_new_iter(MNT_ITER_FORWARD);
		std::vector<FsEntry> value;
		if (iter == NULL) {
			return PxResult::Result<std::vector<FsEntry>>("PxMount::Tab_List / mnt_new_iter", errno);
		}
		DEFER(free, mnt_free_iter(iter));

		struct libmnt_fs* cur_fs = NULL;
		int res2 = mnt_table_first_fs(table, &cur_fs);
		
		if (res2 == 1) return value;
		else if (res2 != 0) {
			return PxResult::FResult("PxMount::Tab_List / mnt_table_first_fs", errno);
		}
		if (cur_fs == NULL) return value;

		if (mnt_table_set_iter(table, iter, cur_fs) < 0) {
			return PxResult::FResult("PxMount::Tab_List / mnt_table_set_iter", errno);
		}
		
		PxResult::Result<FsEntry> pxfs_res("", 0);
		int res = 0;
		// call once to avoid duplicate value
		if (mnt_table_next_fs(table, iter, &cur_fs) == 1) return value;
		for (; res == 0; res = mnt_table_next_fs(table, iter, &cur_fs)) {	
			pxfs_res = MakeFsEntry(cur_fs);
			PXASSERTM(pxfs_res, "PxMount::Tab_List (iter)");
			value.push_back(pxfs_res.assert());
		}
		if (res < 0) {
			return PxResult::FResult("PxMount::Tab_List / mnt_table_first_fs", errno);
		}
		return value;
	}
	PxResult::Result<void> SimpleMount(std::string src, std::string dest, std::string type, unsigned long flags, std::string data) {
		if (mount(src.c_str(), dest.c_str(), type.c_str(), flags, data.c_str()) < 0) {
			return PxResult::Result<void>("PxMount::SimpleMount / mount", errno);
		}
		return PxResult::Null;
	}
	PxResult::Result<void> Mount(std::string src, std::string dest, std::string type, std::string flags) {
		if (src == "" && dest == "") {
			return PxResult::Result<void>("PxMount::Mount", EINVAL);
		}
		struct libmnt_context* ctx = mnt_new_context();
		if (ctx == NULL) {
			return PxResult::Result<void>("PxMount::Mount / mnt_new_context", errno);
		}
		DEFER(freectx, mnt_free_context(ctx));

		if (flags != "") {
			if (mnt_context_set_options(ctx, flags.c_str()) != 0) {
				return PxResult::Result<void>("PxMount::Mount / mnt_context_set_options(\""+flags+"\")", errno);
			}
		}
		if (src != "") {
			if (mnt_context_set_source(ctx, src.c_str()) != 0) {
				return PxResult::Result<void>("PxMount::Mount / mnt_context_set_source(\""+src+"\")", errno);
			}
		}
		if (dest != "") {
			if (mnt_context_set_target(ctx, dest.c_str()) != 0) {
				return PxResult::Result<void>("PxMount::Mount / mnt_context_set_target(\""+dest+"\")", errno);
			}
		}
		if (type != "") {
			if (mnt_context_set_fstype(ctx, type.c_str()) != 0) {
				return PxResult::Result<void>("PxMount::Mount / mnt_context_set_fstype(\""+type+"\")", errno);
			}
		}
		if (mnt_context_mount(ctx)) {
			error_t nerrno;
			if (mnt_context_syscall_called(ctx)) nerrno = mnt_context_get_syscall_errno(ctx);
			else nerrno = 1000;

			return PxResult::FResult("PxMount::Mount / mnt_context_mount", nerrno);
		}
		return PxResult::Null;
	}
	PxResult::Result<void> Unmount(std::string target) {
		struct libmnt_context* ctx = mnt_new_context();
		if (ctx == NULL) {
			return PxResult::Result<void>("PxMount::Mount / mnt_new_context", errno);
		}
		struct libmnt_fs* fs = NULL;
		int ret = mnt_context_find_umount_fs(ctx, target.c_str(), &fs);
		if (ret != 0 || fs == NULL) {
			mnt_free_context(ctx);
			return PxResult::Result<void>("PxMount::Unmount / mnt_context_find_umount_fs", errno);
		}
		mnt_context_set_fs(ctx, fs);
		if (mnt_context_umount(ctx) != 0) {
			mnt_free_context(ctx);
			return PxResult::Result<void>("PxMount::Unmount / mnt_context_umount", errno);
		}
		mnt_free_context(ctx);
		return PxResult::Null;
	}
}
