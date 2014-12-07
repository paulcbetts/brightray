// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

#include "browser/browser_context.h"

#include "browser/download_manager_delegate.h"
#include "browser/inspectable_web_contents_impl.h"
#include "browser/network_delegate.h"
#include "common/application_info.h"

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#endif

using content::BrowserThread;

namespace brightray {

class BrowserContext::ResourceContext : public content::ResourceContext {
 public:
  ResourceContext() : getter_(nullptr) {}

  void set_url_request_context_getter(URLRequestContextGetter* getter) {
    getter_ = getter;
  }

 private:
  virtual net::HostResolver* GetHostResolver() override {
    return getter_->host_resolver();
  }

  virtual net::URLRequestContext* GetRequestContext() override {
    return getter_->GetURLRequestContext();
  }

  URLRequestContextGetter* getter_;
};

BrowserContext::BrowserContext() : resource_context_(new ResourceContext) {
}

void BrowserContext::Initialize() {
  base::FilePath path;
#if defined(OS_LINUX)
  scoped_ptr<base::Environment> env(base::Environment::Create());
  path = base::nix::GetXDGDirectory(env.get(),
                                    base::nix::kXdgConfigHomeEnvVar,
                                    base::nix::kDotConfigDir);
#else
  CHECK(PathService::Get(base::DIR_APP_DATA, &path));
#endif

  path_ = path.Append(base::FilePath::FromUTF8Unsafe(GetApplicationName()));

  auto prefs_path = GetPath().Append(FILE_PATH_LITERAL("Preferences"));
  base::PrefServiceFactory prefs_factory;
  prefs_factory.SetUserPrefsFile(prefs_path,
      JsonPrefStore::GetTaskRunnerForFile(
          prefs_path, BrowserThread::GetBlockingPool()).get());

  auto registry = make_scoped_refptr(new PrefRegistrySimple);
  RegisterInternalPrefs(registry.get());
  RegisterPrefs(registry.get());

  prefs_ = prefs_factory.Create(registry.get());
}

BrowserContext::~BrowserContext() {
  BrowserThread::DeleteSoon(BrowserThread::IO,
                            FROM_HERE,
                            resource_context_.release());
}

void BrowserContext::RegisterInternalPrefs(PrefRegistrySimple* registry) {
  InspectableWebContentsImpl::RegisterPrefs(registry);
}

net::URLRequestContextGetter* BrowserContext::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector protocol_interceptors) {
  DCHECK(!url_request_getter_.get());
  url_request_getter_ = new URLRequestContextGetter(
      this,
      GetPath(),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::FILE),
      protocol_handlers,
      protocol_interceptors.Pass());
  resource_context_->set_url_request_context_getter(url_request_getter_.get());
  return url_request_getter_.get();
}

net::NetworkDelegate* BrowserContext::CreateNetworkDelegate() {
  return new NetworkDelegate;
}

base::FilePath BrowserContext::GetPath() const {
  return path_;
}

bool BrowserContext::IsOffTheRecord() const {
  return false;
}

net::URLRequestContextGetter* BrowserContext::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* BrowserContext::GetRequestContextForRenderProcess(
    int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter* BrowserContext::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    BrowserContext::GetMediaRequestContextForRenderProcess(
        int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    BrowserContext::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return GetRequestContext();
}

content::ResourceContext* BrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate* BrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_)
    download_manager_delegate_.reset(new DownloadManagerDelegate);
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* BrowserContext::GetGuestManager() {
  return NULL;
}

storage::SpecialStoragePolicy* BrowserContext::GetSpecialStoragePolicy() {
  return NULL;
}

content::PushMessagingService* BrowserContext::GetPushMessagingService() {
  return NULL;
}

content::SSLHostStateDelegate* BrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

}  // namespace brightray
