#include <iostream>
#include <thread>
#include <chrono>

#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/file_storage.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/storage_defs.hpp>
#include <libtorrent/storage.hpp>
#include <libtorrent/io.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_info.hpp>

#include <boost/progress.hpp>

namespace lt = libtorrent;

struct temp_storage : lt::storage_interface {
	temp_storage(lt::file_storage const& fs, const std::string tp) : m_files(fs), target_partition(tp) {}
	// Open disk fd
	void initialize(lt::storage_error& se)
	{
		this->fd = open(target_partition.c_str(), O_RDWR | O_CREAT);
		if(this->fd < 0){
			// Failed handle
			std::cerr << "Failed to open " << target_partition << std::endl;

			// TODO exit
		}
		return;
	}

	// assume no resume
	bool has_any_file(lt::storage_error& ec) { return false; }

	int readv(lt::file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, lt::storage_error& ec)
	{
		//return preadv(this->fd, bufs, num_bufs, piece * std::uint64_t(m_files.piece_length()) + offset);
		int index = 0, i;
		int ret = 0;
		char* filename;
		unsigned long long device_offset = 0;
		unsigned long long piece_sum = 0;
		unsigned long long top = 0;
		unsigned long long read_len = 0;
		index = m_files.file_index_at_offset( piece * std::uint64_t(m_files.piece_length()) + offset);
		filename = (char *)malloc( m_files.file_name_len(index));
		memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
		sscanf(filename,"%llx", &device_offset);
		free(filename);
		for( i = 0 ; i < index; i++ )
			piece_sum += m_files.file_size(i);
		for( i = 0 ; i < num_bufs ; i ++){
			read_len += bufs[i].iov_len;
		}
		top = offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum + read_len;
		if ( top > m_files.file_size(index) )
		{
			unsigned long long overflow = top - m_files.file_size(index);
			unsigned long long remind = read_len - overflow;
			ret = preadv(this->fd, bufs, remind, device_offset + offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum);
			index = m_files.file_index_at_offset( piece * std::uint64_t(m_files.piece_length()) + offset + read_len);
			filename = (char *)malloc( m_files.file_name_len(index));
			memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
			sscanf(filename,"%llx", &device_offset);
			free(filename);
			ret += preadv(this->fd, bufs, overflow, device_offset);
		}
		else{
			ret = preadv(this->fd, bufs, num_bufs, device_offset + offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum);
		}
		return ret;
	}

	int writev(lt::file::iovec_t const* bufs, int num_bufs, int piece, int offset, int flags, lt::storage_error& ec)
	{
		int index = 0, i;
		int ret = 0;
		char* filename;
		unsigned long long device_offset = 0;
		unsigned long long piece_sum = 0;
		unsigned long long top = 0;
		unsigned long long write_len = 0;
		index = m_files.file_index_at_offset( piece * std::uint64_t(m_files.piece_length()) + offset);
		filename = (char *)malloc( m_files.file_name_len(index));
		memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
		sscanf(filename,"%llx", &device_offset);
		free(filename);
		for( i = 0 ; i < index; i++ )
			piece_sum += m_files.file_size(i);
		for( i = 0 ; i < num_bufs ; i ++){
			write_len += bufs[i].iov_len;
		}
		top = offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum + write_len;
		if ( top > m_files.file_size(index) )
		{
			unsigned long long overflow = top - m_files.file_size(index);
			unsigned long long remind = write_len - overflow;
			ret = pwritev(this->fd, bufs, remind, device_offset + offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum);
			index = m_files.file_index_at_offset( piece * std::uint64_t(m_files.piece_length()) + offset + write_len);
			filename = (char *)malloc( m_files.file_name_len(index));
			memcpy( filename, m_files.file_name_ptr(index), m_files.file_name_len(index));
			sscanf(filename,"%llx", &device_offset);
			free(filename);
			ret += pwritev(this->fd, bufs, overflow, device_offset);
		}
		else{
			ret = pwritev(this->fd, bufs, num_bufs, device_offset + offset + piece * std::uint64_t(m_files.piece_length()) - piece_sum);
		}
		return ret;
	}

	// Not need
	void rename_file(int index, std::string const& new_filename, lt::storage_error& ec)
	{ assert(false); return ; }

	int move_storage(std::string const& save_path, int flags, lt::storage_error& ec) { return 0; }
	bool verify_resume_data(lt::bdecode_node const& rd
					, std::vector<std::string> const* links
					, lt::storage_error& error) { return false; }
	void write_resume_data(lt::entry& rd, lt::storage_error& ec) const { return ; }
	void set_file_priority(std::vector<boost::uint8_t> const& prio, lt::storage_error& ec) {return ;}
	void release_files(lt::storage_error& ec) { return ; }
	void delete_files(int i, lt::storage_error& ec) { return ; }

	bool tick () { return false; };


	lt::file_storage m_files;
	int fd;
	const std::string target_partition;
};


lt::storage_interface* temp_storage_constructor(lt::storage_params const& params)
{
	return new temp_storage(*params.files, params.path);
}

int main(int argc, char const* argv[])
{
	if (argc != 3) {
		std::cerr << "usage: " << argv[0] << " <magnet-url/torrent-file> <target-partition-path>" << std::endl;
		return 1;
	}
	lt::session ses;
	lt::error_code ec;
	lt::settings_pack set;

	// setting
	// we don't need DHT
	set.set_bool(lt::settings_pack::enable_dht, false);
	ses.apply_settings(set);

	lt::add_torrent_params atp;

	// magnet or torrent
	// TODO find a better way
	std::string bt_info = argv[1];
	if(bt_info.substr(bt_info.length() - 8, 8) == ".torrent"){
		atp.ti = boost::make_shared<lt::torrent_info>(bt_info, boost::ref(ec), 0);
	}
	else{
		atp.url = argv[1];
	}
	atp.save_path = argv[2];
	atp.storage = temp_storage_constructor;

	lt::torrent_handle handle = ses.add_torrent(atp);
	//boost::progress_display show_progress(100, std::cout);
	unsigned long last_progess = 0, progress = 0;
	lt::torrent_status status;

	for(;;){
		std::vector<lt::alert*> alerts;
		ses.pop_alerts(&alerts);

		status = handle.status();
		// progress
		last_progess = progress;
		progress = status.progress * 100;
		//show_progress += progress - last_progess;
		std::cout << "\r"
			<< "[T: " << progress << "%] "
			<< "[D: " << (float)status.download_payload_rate / 1024 / 1024 << "MB/s] "
			<< "[U: " << (float)status.upload_payload_rate / 1024 / 1024 << "MB/s] "
			<< std::flush;

		for (lt::alert const* a : alerts) {
			// std::cout << a->message() << std::endl;
			// if we receive the finished alert or an error, we're done
			if (lt::alert_cast<lt::torrent_finished_alert>(a)) {
				goto done;
			}
			if (status.is_finished) {
				goto done;
			}
			if (lt::alert_cast<lt::torrent_error_alert>(a)) {
				std::cerr << "Error" << std::endl;
				return 1;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	done:
	std::cout << std::endl;


	// Start high performance seed
	lt::high_performance_seed(set);
	ses.apply_settings(set);
	std::cout << "Start seeding" << std::endl;

	// seed until idle 15mins
	int timeout = 15 * 60;

	// seed until seed rate 300%
	boost::int64_t seeding_rate_limit = 3;
	boost::int64_t total_size = handle.torrent_file()->total_size();

	for (;;) {
		status = handle.status();
		int utime = status.time_since_upload;
		int dtime = status.time_since_download;
		boost::int64_t total_payload_upload = status.total_payload_upload;

		std::cout << "\r"
			<< "[T: " << progress << "%] "
			<< "[D: " << (float)status.download_payload_rate / 1024 / 1024 << "MB/s] "
			<< "[U: " << (float)status.upload_payload_rate / 1024 / 1024 << "MB/s] "
			<< std::flush;

		if(utime == -1 && timeout < dtime){
			break;
		}
		else if(timeout < utime){
			// idle 15mins
			break;
		}
		else if(seeding_rate_limit < (total_payload_upload / total_size)){
			// seeding 300%
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	std::cout << std::endl;
	std::cout << "done, shutting down" << std::endl;

	return 0;
}
