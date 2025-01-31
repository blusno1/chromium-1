/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/filesystem/FileSystemCallbacks.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "bindings/modules/v8/V8EntryCallback.h"
#include "bindings/modules/v8/V8ErrorCallback.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileError.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DOMFilePath.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/DOMFileSystemBase.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/DirectoryReader.h"
#include "modules/filesystem/Entry.h"
#include "modules/filesystem/FileCallback.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileWriterBase.h"
#include "modules/filesystem/FileWriterBaseCallback.h"
#include "modules/filesystem/Metadata.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/FileMetadata.h"
#include "public/platform/WebFileWriter.h"

namespace blink {

FileSystemCallbacksBase::FileSystemCallbacksBase(
    ErrorCallbackBase* error_callback,
    DOMFileSystemBase* file_system,
    ExecutionContext* context)
    : error_callback_(error_callback),
      file_system_(file_system),
      execution_context_(context) {
  DCHECK(execution_context_);

  if (file_system_)
    file_system_->AddPendingCallbacks();
}

FileSystemCallbacksBase::~FileSystemCallbacksBase() {
  if (file_system_)
    file_system_->RemovePendingCallbacks();
}

void FileSystemCallbacksBase::DidFail(int code) {
  if (error_callback_) {
    InvokeOrScheduleCallback(&ErrorCallbackBase::Invoke,
                             error_callback_.Release(),
                             static_cast<FileError::ErrorCode>(code));
  }
}

bool FileSystemCallbacksBase::ShouldScheduleCallback() const {
  return !ShouldBlockUntilCompletion() && execution_context_ &&
         execution_context_->IsContextPaused();
}

template <typename CB, typename CBArg>
void FileSystemCallbacksBase::HandleEventOrScheduleCallback(CB* callback,
                                                            CBArg* arg) {
  DCHECK(callback);
  if (callback) {
    if (ShouldScheduleCallback()) {
      DOMFileSystem::ScheduleCallback(
          execution_context_.Get(),
          WTF::Bind(&CB::handleEvent, WrapPersistent(callback),
                    WrapPersistent(arg)));
    } else {
      callback->handleEvent(arg);
    }
  }
  execution_context_.Clear();
}

template <typename CB>
void FileSystemCallbacksBase::HandleEventOrScheduleCallback(CB* callback) {
  DCHECK(callback);
  if (callback) {
    if (ShouldScheduleCallback()) {
      DOMFileSystem::ScheduleCallback(
          execution_context_.Get(),
          WTF::Bind(&CB::handleEvent, WrapPersistent(callback)));
    } else {
      callback->handleEvent();
    }
  }
  execution_context_.Clear();
}

template <typename CallbackMemberFunction,
          typename CallbackClass,
          typename... Args>
void FileSystemCallbacksBase::InvokeOrScheduleCallback(
    CallbackMemberFunction&& callback_member_function,
    CallbackClass&& callback_object,
    Args&&... args) {
  DCHECK(callback_object);

  if (ShouldScheduleCallback()) {
    DOMFileSystem::ScheduleCallback(
        execution_context_.Get(),
        WTF::Bind(callback_member_function, WrapPersistent(callback_object),
                  WrapPersistentIfNeeded(args)...));
  } else {
    ((*callback_object).*callback_member_function)(args...);
  }
  execution_context_.Clear();
}

// ScriptErrorCallback --------------------------------------------------------

// static
ScriptErrorCallback* ScriptErrorCallback::Wrap(V8ErrorCallback* callback) {
  // DOMFileSystem operations take an optional (nullable) callback. If a
  // script callback was not passed, don't bother creating a dummy wrapper
  // and checking during invoke().
  if (!callback)
    return nullptr;
  return new ScriptErrorCallback(callback);
}

void ScriptErrorCallback::Trace(blink::Visitor* visitor) {
  ErrorCallbackBase::Trace(visitor);
  visitor->Trace(callback_);
}

void ScriptErrorCallback::Invoke(FileError::ErrorCode error) {
  callback_->handleEvent(FileError::CreateDOMException(error));
};

ScriptErrorCallback::ScriptErrorCallback(V8ErrorCallback* callback)
    : callback_(callback) {}

// EntryCallbacks -------------------------------------------------------------

void EntryCallbacks::OnDidGetEntryV8Impl::Trace(blink::Visitor* visitor) {
  visitor->Trace(callback_);
  OnDidGetEntryCallback::Trace(visitor);
}

void EntryCallbacks::OnDidGetEntryV8Impl::OnSuccess(Entry* entry) {
  callback_->handleEvent(entry);
}

std::unique_ptr<AsyncFileSystemCallbacks> EntryCallbacks::Create(
    OnDidGetEntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system,
    const String& expected_path,
    bool is_directory) {
  return base::WrapUnique(new EntryCallbacks(success_callback, error_callback,
                                             context, file_system,
                                             expected_path, is_directory));
}

EntryCallbacks::EntryCallbacks(OnDidGetEntryCallback* success_callback,
                               ErrorCallbackBase* error_callback,
                               ExecutionContext* context,
                               DOMFileSystemBase* file_system,
                               const String& expected_path,
                               bool is_directory)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback),
      expected_path_(expected_path),
      is_directory_(is_directory) {
  DCHECK(success_callback_);
}

void EntryCallbacks::DidSucceed() {
  Entry* entry = is_directory_ ? static_cast<Entry*>(DirectoryEntry::Create(
                                     file_system_, expected_path_))
                               : static_cast<Entry*>(FileEntry::Create(
                                     file_system_, expected_path_));
  InvokeOrScheduleCallback(&OnDidGetEntryCallback::OnSuccess,
                           success_callback_.Release(), entry);
}

// EntriesCallbacks -----------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> EntriesCallbacks::Create(
    DirectoryReaderOnDidReadCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DirectoryReaderBase* directory_reader,
    const String& base_path) {
  return base::WrapUnique(new EntriesCallbacks(
      success_callback, error_callback, context, directory_reader, base_path));
}

EntriesCallbacks::EntriesCallbacks(
    DirectoryReaderOnDidReadCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DirectoryReaderBase* directory_reader,
    const String& base_path)
    : FileSystemCallbacksBase(error_callback,
                              directory_reader->Filesystem(),
                              context),
      success_callback_(success_callback),
      directory_reader_(directory_reader),
      base_path_(base_path) {
  DCHECK(directory_reader_);
}

void EntriesCallbacks::DidReadDirectoryEntry(const String& name,
                                             bool is_directory) {
  if (is_directory)
    entries_.push_back(
        DirectoryEntry::Create(directory_reader_->Filesystem(),
                               DOMFilePath::Append(base_path_, name)));
  else
    entries_.push_back(
        FileEntry::Create(directory_reader_->Filesystem(),
                          DOMFilePath::Append(base_path_, name)));
}

void EntriesCallbacks::DidReadDirectoryEntries(bool has_more) {
  directory_reader_->SetHasMoreEntries(has_more);
  EntryHeapVector entries;
  entries.swap(entries_);
  // FIXME: delay the callback iff shouldScheduleCallback() is true.
  if (success_callback_)
    success_callback_->OnDidReadDirectoryEntries(entries);
}

// FileSystemCallbacks --------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> FileSystemCallbacks::Create(
    FileSystemCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    FileSystemType type) {
  return base::WrapUnique(
      new FileSystemCallbacks(success_callback, error_callback, context, type));
}

FileSystemCallbacks::FileSystemCallbacks(FileSystemCallback* success_callback,
                                         ErrorCallbackBase* error_callback,
                                         ExecutionContext* context,
                                         FileSystemType type)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      success_callback_(success_callback),
      type_(type) {}

void FileSystemCallbacks::DidOpenFileSystem(const String& name,
                                            const KURL& root_url) {
  if (success_callback_)
    HandleEventOrScheduleCallback(
        success_callback_.Release(),
        DOMFileSystem::Create(execution_context_.Get(), name, type_, root_url));
}

// ResolveURICallbacks --------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> ResolveURICallbacks::Create(
    OnDidGetEntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return base::WrapUnique(
      new ResolveURICallbacks(success_callback, error_callback, context));
}

ResolveURICallbacks::ResolveURICallbacks(
    OnDidGetEntryCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      success_callback_(success_callback) {
  DCHECK(success_callback_);
}

void ResolveURICallbacks::DidResolveURL(const String& name,
                                        const KURL& root_url,
                                        FileSystemType type,
                                        const String& file_path,
                                        bool is_directory) {
  DOMFileSystem* filesystem =
      DOMFileSystem::Create(execution_context_.Get(), name, type, root_url);
  DirectoryEntry* root = filesystem->root();

  String absolute_path;
  if (!DOMFileSystemBase::PathToAbsolutePath(type, root, file_path,
                                             absolute_path)) {
    DidFail(FileError::kInvalidModificationErr);
    return;
  }

  Entry* entry =
      is_directory
          ? static_cast<Entry*>(
                DirectoryEntry::Create(filesystem, absolute_path))
          : static_cast<Entry*>(FileEntry::Create(filesystem, absolute_path));
  InvokeOrScheduleCallback(&OnDidGetEntryCallback::OnSuccess,
                           success_callback_.Release(), entry);
}

// MetadataCallbacks ----------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> MetadataCallbacks::Create(
    MetadataCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system) {
  return base::WrapUnique(new MetadataCallbacks(
      success_callback, error_callback, context, file_system));
}

MetadataCallbacks::MetadataCallbacks(MetadataCallback* success_callback,
                                     ErrorCallbackBase* error_callback,
                                     ExecutionContext* context,
                                     DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback) {}

void MetadataCallbacks::DidReadMetadata(const FileMetadata& metadata) {
  if (success_callback_)
    HandleEventOrScheduleCallback(success_callback_.Release(),
                                  Metadata::Create(metadata));
}

// FileWriterBaseCallbacks ----------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> FileWriterBaseCallbacks::Create(
    FileWriterBase* file_writer,
    FileWriterBaseCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return base::WrapUnique(new FileWriterBaseCallbacks(
      file_writer, success_callback, error_callback, context));
}

FileWriterBaseCallbacks::FileWriterBaseCallbacks(
    FileWriterBase* file_writer,
    FileWriterBaseCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, nullptr, context),
      file_writer_(file_writer),
      success_callback_(success_callback) {}

void FileWriterBaseCallbacks::DidCreateFileWriter(
    std::unique_ptr<WebFileWriter> file_writer,
    long long length) {
  file_writer_->Initialize(std::move(file_writer), length);
  if (success_callback_)
    HandleEventOrScheduleCallback(success_callback_.Release(),
                                  file_writer_.Release());
}

// SnapshotFileCallback -------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> SnapshotFileCallback::Create(
    DOMFileSystemBase* filesystem,
    const String& name,
    const KURL& url,
    FileCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context) {
  return base::WrapUnique(new SnapshotFileCallback(
      filesystem, name, url, success_callback, error_callback, context));
}

SnapshotFileCallback::SnapshotFileCallback(DOMFileSystemBase* filesystem,
                                           const String& name,
                                           const KURL& url,
                                           FileCallback* success_callback,
                                           ErrorCallbackBase* error_callback,
                                           ExecutionContext* context)
    : FileSystemCallbacksBase(error_callback, filesystem, context),
      name_(name),
      url_(url),
      success_callback_(success_callback) {}

void SnapshotFileCallback::DidCreateSnapshotFile(
    const FileMetadata& metadata,
    scoped_refptr<BlobDataHandle> snapshot) {
  if (!success_callback_)
    return;

  // We can't directly use the snapshot blob data handle because the content
  // type on it hasn't been set.  The |snapshot| param is here to provide a a
  // chain of custody thru thread bridging that is held onto until *after* we've
  // coined a File with a new handle that has the correct type set on it. This
  // allows the blob storage system to track when a temp file can and can't be
  // safely deleted.

  HandleEventOrScheduleCallback(
      success_callback_.Release(),
      DOMFileSystemBase::CreateFile(metadata, url_, file_system_->GetType(),
                                    name_));
}

// VoidCallbacks --------------------------------------------------------------

std::unique_ptr<AsyncFileSystemCallbacks> VoidCallbacks::Create(
    VoidCallback* success_callback,
    ErrorCallbackBase* error_callback,
    ExecutionContext* context,
    DOMFileSystemBase* file_system) {
  return base::WrapUnique(new VoidCallbacks(success_callback, error_callback,
                                            context, file_system));
}

VoidCallbacks::VoidCallbacks(VoidCallback* success_callback,
                             ErrorCallbackBase* error_callback,
                             ExecutionContext* context,
                             DOMFileSystemBase* file_system)
    : FileSystemCallbacksBase(error_callback, file_system, context),
      success_callback_(success_callback) {}

void VoidCallbacks::DidSucceed() {
  if (success_callback_)
    HandleEventOrScheduleCallback(success_callback_.Release());
}

}  // namespace blink
