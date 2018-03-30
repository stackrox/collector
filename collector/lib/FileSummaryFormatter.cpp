/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include "FileSummaryFormatter.h"
#include "EventMap.h"
#include "Logging.h"

#include <uuid/uuid.h>

namespace collector {

namespace {

static EventMap<data::FileSummary_FileOp> file_ops = {
  {
    // /* PPME_SYSCALL_OPEN_X */{"open", EC_FILE, EF_CREATES_FD | EF_MODIFIES_STATE, 4, {{"fd", PT_FD, PF_DEC}, {"name", PT_FSPATH, PF_NA}, {"flags", PT_FLAGS32, PF_HEX, file_flags}, {"mode", PT_UINT32, PF_OCT} } },
    // path=fd.name
    { "open<", data::FileSummary::OPEN },

    // /* PPME_SYSCALL_OPENAT_X */{"openat", EC_FILE, EF_CREATES_FD | EF_MODIFIES_STATE, 1, {{"fd", PT_FD, PF_DEC} } },
    // path=fd.name
    { "openat<", data::FileSummary::OPEN },

    // /* PPME_SYSCALL_CREAT_X */{"creat", EC_FILE, EF_CREATES_FD | EF_MODIFIES_STATE, 3, {{"fd", PT_FD, PF_DEC}, {"name", PT_FSPATH, PF_NA}, {"mode", PT_UINT32, PF_HEX} } },
    // path=fd.name
    { "creat<", data::FileSummary::CREATE },
    
    // /* PPME_SYSCALL_MKDIR_E */{"mkdir", EC_FILE, EF_NONE, 2, {{"path", PT_FSPATH, PF_NA}, {"mode", PT_UINT32, PF_HEX} } },
    // /* PPME_SYSCALL_MKDIR_2_X */{"mkdir", EC_FILE, EF_NONE, 2, {{"res", PT_ERRNO, PF_DEC}, {"path", PT_FSPATH, PF_NA} } },
    // path=evt.args.path
    { "mkdir", data::FileSummary::CREATE },

    // /* PPME_SYSCALL_RENAME_X */{"rename", EC_FILE, EF_NONE, 3, {{"res", PT_ERRNO, PF_DEC}, {"oldpath", PT_FSPATH, PF_NA}, {"newpath", PT_FSPATH, PF_NA} } },
    // path=evt.args.oldpath, new_path=evt.args.newpath
    { "rename<", data::FileSummary::MOVE },

    // /* PPME_SYSCALL_RENAMEAT_X */{"renameat", EC_FILE, EF_NONE, 5, {{"res", PT_ERRNO, PF_DEC}, {"olddirfd", PT_FD, PF_DEC}, {"oldpath", PT_CHARBUF, PF_NA}, {"newdirfd", PT_FD, PF_DEC}, {"newpath", PT_CHARBUF, PF_NA} } },
    // path=(evt.args.oldfd, evt.args.oldpath), new_path=(evt.args.oldfd, evt.args.newpath)
    { "renameat<", data::FileSummary::MOVE },

    // /* PPME_SYSCALL_RMDIR_E */{"rmdir", EC_FILE, EF_NONE, 1, {{"path", PT_FSPATH, PF_NA} } },
    // /* PPME_SYSCALL_RMDIR_2_X */{"rmdir", EC_FILE, EF_NONE, 2, {{"res", PT_ERRNO, PF_DEC}, {"path", PT_FSPATH, PF_NA} } },
    // path=evt.args.path
    { "rmdir", data::FileSummary::REMOVE },

    // /* PPME_SYSCALL_UNLINK_E */{"unlink", EC_FILE, EF_NONE, 1, {{"path", PT_FSPATH, PF_NA} } },
    // path=evt.args.path
    { "unlink>", data::FileSummary::REMOVE },

    // /* PPME_SYSCALL_UNLINKAT_E */{"unlinkat", EC_FILE, EF_NONE, 2, {{"dirfd", PT_FD, PF_DEC}, {"name", PT_CHARBUF, PF_NA} } },
    // path=(evt.args.dirfd, evt.args.name)
    { "unlinkat>", data::FileSummary::REMOVE },
    
    // /* PPME_SYSCALL_SYMLINK_X */{"symlink", EC_FILE, EF_NONE, 3, {{"res", PT_ERRNO, PF_DEC}, {"target", PT_CHARBUF, PF_NA}, {"linkpath", PT_FSPATH, PF_NA} } },
    // path=evt.args.target, new_path=evt.args.linkpath 
    { "symlink<", data::FileSummary::LINK },

    // /* PPME_SYSCALL_SYMLINKAT_X */{"symlinkat", EC_FILE, EF_NONE, 4, {{"res", PT_ERRNO, PF_DEC}, {"target", PT_CHARBUF, PF_NA}, {"linkdirfd", PT_FD, PF_DEC}, {"linkpath", PT_CHARBUF, PF_NA} } },
    // path=evt.args.target, new_path=(evt.args.linkdirfd,evt.args.linkpath)
    { "symlinkat<", data::FileSummary::LINK },

    // /* PPME_SYSCALL_LINK_E */{"link", EC_FILE, EF_NONE, 2, {{"oldpath", PT_FSPATH, PF_NA}, {"newpath", PT_FSPATH, PF_NA} } },
    // path=event.args.oldpath, new_path=event.args.newpath
    { "link>", data::FileSummary::LINK },

    // /* PPME_SYSCALL_LINKAT_E */{"linkat", EC_FILE, EF_NONE, 4, {{"olddir", PT_FD, PF_DEC}, {"oldpath", PT_CHARBUF, PF_NA}, {"newdir", PT_FD, PF_DEC}, {"newpath", PT_CHARBUF, PF_NA} } },
    // path=(evt.args.olddir,event.args.oldpath), new_path=(evt.args.newdir,event.args.newpath)
    { "linkat>", data::FileSummary::LINK },
  },
  data::FileSummary::FileSummary::UNSET,
};

}

using FileContainer = data::FileSummary::Container;
using FileDetails = data::FileSummary::File;
using FileOp = data::FileSummary::FileOp;
using FileOperation = data::FileSummary::FileOperation;
using FileProcess = data::FileSummary::Process;
using FileSummary = data::FileSummary;

const data::FileSummary* FileSummaryFormatter::ToProtoMessage(sinsp_evt* event) {
  FileOp op = file_ops[event->get_type()];
  if (op == FileSummary::UNSET) return nullptr;

  if (event->get_type() == PPME_SYSCALL_MKDIR_X ||
      event->get_type() == PPME_SYSCALL_MKDIR_2_E ||
      event->get_type() == PPME_SYSCALL_RMDIR_X ||
      event->get_type() == PPME_SYSCALL_RMDIR_2_E) {
    return nullptr;
  }

  FileSummary* file_summary = AllocateRoot();
  AddFileOperations(event, file_summary, op);

  file_summary->set_allocated_container(CreateFileContainer(event));
  file_summary->set_allocated_process(CreateFileProcess(event));
  file_summary->set_allocated_file(CreateFileDetails(event));

  file_summary->set_start_time(event->get_ts());
  file_summary->set_end_time(event->get_ts());

  uuid_t uuid;
  uuid_generate(uuid);

  constexpr int kUuidStringLength = 36;
  char uuid_str[kUuidStringLength + 1];
  uuid_unparse_lower(uuid, uuid_str);
  file_summary->mutable_id()->set_value(uuid_str, kUuidStringLength);

  return file_summary;
}

FileOperation* FileSummaryFormatter::CreateFileOperation(sinsp_evt* event, FileOp op) {
  auto file_operation = Allocate<FileOperation>();
  file_operation->set_operation(op);
  return file_operation;
}

void FileSummaryFormatter::AddFileOperations(sinsp_evt* event, FileSummary* file_summary, FileOp op) {
  file_summary->mutable_ops()->AddAllocated(CreateFileOperation(event, op));

  // For successful open/openat syscalls, add READ|WRITE|CREATE file operations based on flags
  if (event->get_type() == PPME_SYSCALL_OPEN_X ||
      event->get_type() == PPME_SYSCALL_OPENAT_X) {
    if (const int64_t* res = event_extractor_.get_event_rawres(event)) {
      if (*res >= 0) {
        if (const int32_t* flags = event_extractor_.get_evt_arg_flags(event)) {
          if (*flags & PPM_O_CREAT)
            file_summary->mutable_ops()->AddAllocated(CreateFileOperation(event, FileSummary::CREATE));

          if (*flags & PPM_O_RDONLY) 
            file_summary->mutable_ops()->AddAllocated(CreateFileOperation(event, FileSummary::READ));

          if (*flags & PPM_O_WRONLY)
            file_summary->mutable_ops()->AddAllocated(CreateFileOperation(event, FileSummary::WRITE));
        }
      }
    }
  } else if (event->get_type() == PPME_SYSCALL_CREAT_X) {
    // creat() is equivalent to open() with flags equal to O_CREAT|O_WRONLY|O_TRUNC
    file_summary->mutable_ops()->AddAllocated(CreateFileOperation(event, FileSummary::WRITE));
  }
}

FileContainer* FileSummaryFormatter::CreateFileContainer(sinsp_evt* event) {
  const std::string* container_id = event_extractor_.get_container_id(event);
  if (!container_id) return nullptr;

  auto file_container = Allocate<FileContainer>();
  if (container_id->length() <= 12) {
    file_container->set_id(*container_id);
  } else {
    file_container->set_id(container_id->substr(0,12));
  }

  if (const uint32_t* privileged = event_extractor_.get_container_privileged(event)) {
    file_container->set_privileged(*privileged != 0);
  }
  return file_container;
}

FileDetails* FileSummaryFormatter::CreateFileDetails(sinsp_evt* event) {
  auto file_details = Allocate<FileDetails>();

  switch (event->get_type()) {
    case PPME_SYSCALL_OPEN_X:
    case PPME_SYSCALL_OPENAT_X:
    case PPME_SYSCALL_CREAT_X: 
    {
      if (const char* path = event_extractor_.get_fd_name(event)) {
        file_details->set_path(path);
      }
      if (sinsp_fdinfo_t* fd_info = event->get_fd_info()) {
        file_details->set_directory(fd_info->is_directory());
      }
    } break;

    case PPME_SYSCALL_MKDIR_E:
    case PPME_SYSCALL_MKDIR_2_X:
    case PPME_SYSCALL_RMDIR_E:
    case PPME_SYSCALL_RMDIR_2_X:
      file_details->set_directory(true);
    case PPME_SYSCALL_UNLINK_E:
    {
      if (const char* path = event_extractor_.get_evt_arg_path(event)) {
        file_details->set_path(path);
      }
    } break;

    case PPME_SYSCALL_RENAME_X:
    case PPME_SYSCALL_LINK_E:
    {
      if (const char* oldpath = event_extractor_.get_evt_arg_oldpath(event)) {
        file_details->set_path(oldpath);
      }
      if (const char* newpath = event_extractor_.get_evt_arg_newpath(event)) {
        file_details->set_new_path(newpath);
      }
    } break;

    case PPME_SYSCALL_LINKAT_E:
    {
      const int64_t* old_dir = event_extractor_.get_evt_arg_olddir(event);
      const char* old_path = event_extractor_.get_evt_arg_oldpath(event);
      file_details->set_path(GetPathAtFD(event, old_dir, old_path));

      const int64_t* new_dir = event_extractor_.get_evt_arg_newdir(event);
      const char* new_path = event_extractor_.get_evt_arg_newpath(event);
      file_details->set_new_path(GetPathAtFD(event, new_dir, new_path));
    } break;

    case PPME_SYSCALL_RENAMEAT_X:
    {
      const int64_t* old_dirfd = event_extractor_.get_evt_arg_olddirfd(event);
      const char* old_path = event_extractor_.get_evt_arg_oldpath(event);
      file_details->set_path(GetPathAtFD(event, old_dirfd, old_path));

      const int64_t* new_dirfd = event_extractor_.get_evt_arg_newdirfd(event);
      const char* new_path = event_extractor_.get_evt_arg_newpath(event);
      file_details->set_new_path(GetPathAtFD(event, new_dirfd, new_path));
    } break;
     
    case PPME_SYSCALL_SYMLINK_X:
    {
      if (const char* target = event_extractor_.get_evt_arg_target(event)) {
        file_details->set_path(target);
      }
      if (const char* linkpath = event_extractor_.get_evt_arg_linkpath(event)) {
        file_details->set_new_path(linkpath);
      }
    } break;
 
    case PPME_SYSCALL_SYMLINKAT_X:
    {
      if (const char* target = event_extractor_.get_evt_arg_target(event)) {
        file_details->set_path(target);
      }
      const int64_t* link_dirfd = event_extractor_.get_evt_arg_linkdirfd(event);
      const char* link_path = event_extractor_.get_evt_arg_linkpath(event);
      file_details->set_new_path(GetPathAtFD(event, link_dirfd, link_path));
    } break;

    case PPME_SYSCALL_UNLINKAT_E:
    {
      const int64_t* dirfd = event_extractor_.get_evt_arg_dirfd(event);
      const char* name = event_extractor_.get_evt_arg_name(event);
      file_details->set_path(GetPathAtFD(event, dirfd, name));
    } break;
  }
  
  return file_details;
}

FileProcess* FileSummaryFormatter::CreateFileProcess(sinsp_evt* event) {
  auto file_process = Allocate<FileProcess>();
  if (const char* cmdline = event_extractor_.get_cmdline(event)) file_process->set_cmdline(cmdline);
  if (const uint32_t* uid = event_extractor_.get_uid(event)) file_process->set_uid(*uid);
  if (const uint32_t* gid = event_extractor_.get_gid(event)) file_process->set_gid(*gid);
  return file_process;
}

std::string FileSummaryFormatter::GetPathAtFD(sinsp_evt* event, const int64_t* at_fd, const char* path) {
  std::string result;
  if (!path) return result;
  if (at_fd) {
    if (sinsp_threadinfo* tinfo = event->get_thread_info()) {
      // in some cases fd contains a flag value (AT_FDCWD=-100), and is not properly sign-extended
      if (*at_fd == PPM_AT_FDCWD || *at_fd == ((int64_t)PPM_AT_FDCWD & 0x00000000FFFFFFFF)) {
        result = tinfo->get_cwd();
      } else if (sinsp_fdinfo_t* at_fd_info = tinfo->get_fd(*at_fd)) {
        result = at_fd_info->m_name;
      }
    }
  }
  if (!result.empty() && result.back() != '/') result.push_back('/');
  result.append(path);
  return result;
}

}  // namespace collector
