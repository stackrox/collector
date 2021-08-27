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

#include "DownloadKernelObject.h"

#include "FileDownloader.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

std::string getModuleVersion() {
  std::ifstream file("/kernel-modules/MODULE_VERSION.txt");
  if (!file.is_open()) {
    CLOG(WARNING) << "Failed to open '/kernel-modules/MODULE_VERSION.txt'";
    return "";
  }

  std::string module_version;
  getline(file, module_version);

  return module_version;
}

bool downloadKernelObjectFromURL(FileDownloader& downloader, const std::string& base_url, const std::string& kernel_module, const std::string& module_version) {
  std::string url(base_url + "/" + module_version + "/" + kernel_module + ".gz");

  CLOG(DEBUG) << "Attempting to download kernel object from " << url;

  if (!downloader.SetURL(url)) return false;
  if (!downloader.Download()) return false;

  CLOG(DEBUG) << "Downloaded kernel object from " << url;

  return true;
}

bool downloadKernelObjectFromGRPC(FileDownloader& downloader, const std::string& hostname, const std::string& kernel_module, const std::string& module_version) {
  size_t port_offset = hostname.find(':');
  if (port_offset == std::string::npos) {
    CLOG(WARNING) << "GRPC server must have a valid port";
    return false;
  }

  const std::string SNI_hostname(GetSNIHostname());
  if (SNI_hostname.find(':') != std::string::npos) {
    CLOG(WARNING) << "SNI hostname must NOT specify a port";
    return false;
  }

  std::string server_hostname;
  if (hostname.compare(0, port_offset, SNI_hostname) != 0) {
    const std::string server_port(hostname.substr(port_offset + 1));
    server_hostname = SNI_hostname + ":" + server_port;
    downloader.ConnectTo(SNI_hostname + ":" + server_port + ":" + hostname);
  } else {
    server_hostname = hostname;
  }

  // Attempt to download the kernel object from the GRPC server
  std::string base_url("https://" + server_hostname + "/kernel-objects");
  if (base_url.empty()) return false;

  return downloadKernelObjectFromURL(downloader, base_url, kernel_module, module_version);
}

bool downloadKernelObject(const std::string& hostname, const Json::Value& tls_config, const std::string& kernel_module, const std::string& module_path) {
  FileDownloader downloader;
  if (!downloader.IsReady()) {
    CLOG(WARNING) << "Failed to initialize FileDownloader object";
    return false;
  }

  std::string module_version(getModuleVersion());
  if (module_version.empty()) {
    CLOG(WARNING) << "/kernel-modules/MODULE_VERSION.txt must exist and not be empty";
    return false;
  }

  downloader.IPResolve(FileDownloader::IPv4);
  downloader.SetRetries(30, 1, 60);
  downloader.OutputFile(module_path + ".gz");
  if (!downloader.SetConnectionTimeout(2)) return false;
  if (!downloader.FollowRedirects(true)) return false;

  if (!tls_config.isNull()) {
    if (!downloader.CACert(tls_config["caCertPath"].asCString())) return false;
    if (!downloader.Cert(tls_config["clientCertPath"].asCString())) return false;
    if (!downloader.Key(tls_config["clientKeyPath"].asCString())) return false;
  } else {
    CLOG(WARNING) << "No TLS configuration provided";
    return false;
  }

  if (downloadKernelObjectFromGRPC(downloader, hostname, kernel_module, module_version)) {
    return true;
  }

  std::string base_url(getModuleDownloadBaseURL());
  if (base_url.empty()) {
    return false;
  }

  downloader.ResetCURL();
  downloader.IPResolve(FileDownloader::IPv4);
  downloader.SetRetries(30, 1, 60);
  downloader.OutputFile(module_path + ".gz");
  if (!downloader.SetConnectionTimeout(2)) return false;
  if (!downloader.FollowRedirects(true)) return false;

  if (downloadKernelObjectFromURL(downloader, base_url, kernel_module, module_version)) {
    return true;
  }
  return false;
}
}  // namespace collector
