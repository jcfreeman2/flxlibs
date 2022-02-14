/**
 * @file test_elinkhandler_app.cxx Test application for
 * ElinkConcept and ElinkModel. Inits, starts, stops block parsers.
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "CardWrapper.hpp"
#include "CreateElink.hpp"
#include "ElinkConcept.hpp"
#include "flxlibs/AvailableParserOperations.hpp"

#include "logging/Logging.hpp"
#include "readout/ReadoutTypes.hpp"
#include "readout/RawWIBTp.hpp"

#include "packetformat/block_format.hpp"
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <sstream>

using namespace dunedaq::flxlibs;
using namespace dunedaq::readout;


const constexpr std::size_t USER_PAYLOAD_SIZE = 5568; // for 12: 5568
struct TP_SUPERCHUNK_STRUCT
{
  TP_SUPERCHUNK_STRUCT() {}
  TP_SUPERCHUNK_STRUCT(size_t size, char* data)
    : size(size)
    , data(data)
  {}

  size_t size = 0;
  std::unique_ptr<char> data = nullptr;
};

using LatencyBuffer = folly::ProducerConsumerQueue<TP_SUPERCHUNK_STRUCT>;

int
main(int /*argc*/, char** /*argv[]*/)
{
  // Run marker
  std::atomic<bool> marker{ true };

  // Killswitch that flips the run marker
  auto killswitch = std::thread([&]() {
    TLOG() << "Application will terminate in 60s...";
    std::this_thread::sleep_for(std::chrono::seconds(60));
    marker.store(false);
  });

  // Dummy command
  nlohmann::json cmd_params = "{}"_json;

  // Counter
  std::atomic<int> block_counter{ 0 };

  // CardWrapper
  TLOG() << "Creating CardWrapper...";
  CardWrapper flx;
  std::map<int, std::unique_ptr<ElinkConcept>> elinks;

  TLOG() << "Creating Elink models...";
  // 5 elink handlers
  for (int i = 0; i < 5; ++i) {
    //elinks[i * 64] = createElinkModel("wib");
    TLOG() << "Elink " << i << "...";
    elinks[i * 64] = std::make_unique<ElinkModel<TP_SUPERCHUNK_STRUCT>>();  
    auto& handler = elinks[i * 64];
    handler->init(cmd_params, 100000);
    handler->conf(cmd_params, 4096, true);
    handler->start(cmd_params);
  }

  // Add TP link
  // elinks[5 * 64] = createElinkModel("raw_tp");
  // auto& tphandler = elinks[5 * 64];
  // tphandler->init(cmd_params, 100000);
  // tphandler->conf(cmd_params, 4096, true);
  TLOG() << "Creating TP link...";
  elinks[5 * 64] = std::make_unique<ElinkModel<TP_SUPERCHUNK_STRUCT>>();
  auto& tphandler = elinks[5 * 64];
  tphandler->init(cmd_params, 100000);
  tphandler->conf(cmd_params, 4096, true);
  std::unique_ptr<folly::ProducerConsumerQueue<TP_SUPERCHUNK_STRUCT>> tpbuffer = std::make_unique<LatencyBuffer>(1000000);


  // Modify a specific elink handler
  bool first = true;
  int firstWIBframes = 0;
  auto& parser0 = elinks[0]->get_parser();
  parser0.process_chunk_func = [&](const felix::packetformat::chunk& chunk) {
    auto subchunk_data = chunk.subchunks();
    auto subchunk_sizes = chunk.subchunk_lengths();
    auto n_subchunks = chunk.subchunk_number();
    types::WIB_SUPERCHUNK_STRUCT wss;
    uint32_t bytes_copied_chunk = 0; // NOLINT 
    for (unsigned i = 0; i < n_subchunks; i++) {
      parsers::dump_to_buffer(
        subchunk_data[i], subchunk_sizes[i], static_cast<void*>(&wss.data), bytes_copied_chunk, sizeof(types::WIB_SUPERCHUNK_STRUCT));
      bytes_copied_chunk += subchunk_sizes[i];
    }

    if (first) {
      TLOG() << "Chunk with length: " << chunk.length();
      TLOG() << "WIB frame timestamp: " << wss.get_timestamp();
      ++firstWIBframes;
      if (firstWIBframes > 100) {
        first = false;
      }
    }
  };


  // Modify TP link handler
  /*bool firstTPshort = true;
  auto& tpparser = elinks[5 * 64]->get_parser();
  tpparser.process_shortchunk_func = [&](const felix::packetformat::shortchunk& shortchunk) {
    if (firstTPshort) {
      TLOG() << "SC Length: " << shortchunk.length;
      firstTPshort = false;
    }
  };
  */

  bool firstTPchunk = true;
  int amount = 0;
  auto& tpparser = elinks[5 * 64]->get_parser();
  uint64_t good_counter = 0;
  uint64_t total_counter = 0;
  tpparser.process_chunk_func = [&](const felix::packetformat::chunk& chunk) {
    ++total_counter;
    if (firstTPchunk) {
      auto subchunk_data = chunk.subchunks();
      auto subchunk_sizes = chunk.subchunk_lengths();
      auto n_subchunks = chunk.subchunk_number();
      auto chunk_length = chunk.length();
      
      TLOG() << "TP subchunk number: " << n_subchunks; 
      TLOG() << "TP chunk length: " << chunk_length;

      uint32_t bytes_copied_chunk = 0;
      dunedaq::detdataformats::RawWIBTp* rwtpp = static_cast<dunedaq::detdataformats::RawWIBTp*>(std::malloc(chunk_length)); //+ sizeof(int)));
      //auto* rwtpip = reinterpret_cast<uint8_t*>(rwtpp);
      
      char* payload = static_cast<char*>(malloc(chunk_length * sizeof(char)));
      TP_SUPERCHUNK_STRUCT payload_struct(chunk_length, payload);
      for (unsigned i = 0; i < n_subchunks; i++) {
        TLOG() << "TP subchunk " << i << " length: " << subchunk_sizes[i];
        parsers::dump_to_buffer(
          subchunk_data[i], subchunk_sizes[i], static_cast<void*>(payload), bytes_copied_chunk, chunk_length);
        bytes_copied_chunk += subchunk_sizes[i];
      }
      if (!tpbuffer->write(std::move(payload_struct))) {
        // Buffer full
      }

      if ((uint32_t)(rwtpp->get_crate_no()) == 21) { // RS FIXME -> read from cmdline the list of signatures loaded to EMU
        ++good_counter;
      }

      std::ostringstream oss;
      //rwtpp->m_head.print(oss);  doesn't work with readout v2.8.2
      oss << "Printing raw WIB TP header:\n";
      oss << "flags:" << unsigned(rwtpp->get_flags()) << " slot:" << unsigned(rwtpp->get_slot_no()) << " wire:" << unsigned(rwtpp->get_wire_no())
          << " fiber:" << unsigned(rwtpp->get_fiber_no()) << " crate:" << unsigned(rwtpp->get_crate_no()) << " timestamp:" << rwtpp->get_timestamp();
      oss << '\n';
      TLOG() << oss.str();

      //stypes::RAW_WIB_TP_STRUCT rwtps;
      //rwtps.rwtp.reset(rwtpp);

      if (amount > 1000) {
        firstTPchunk = false;
      } else {
        amount++;
      }
    }
  };
  tphandler->start(cmd_params);

  // Implement how block addresses should be handled
  std::function<void(uint64_t)> count_block_addr = [&](uint64_t block_addr) { // NOLINT
    ++block_counter;
    const auto* block = const_cast<felix::packetformat::block*>(
      felix::packetformat::block_from_bytes(reinterpret_cast<const char*>(block_addr)) // NOLINT
    );
    auto elink = block->elink;
    if (elinks.count(elink) != 0) {
      if (elinks[elink]->queue_in_block_address(block_addr)) {
        // queued block
      } else {
        // couldn't queue block
      }
    }
  };

  // Set this function as the handler of blocks.
  flx.set_block_addr_handler(count_block_addr);

  TLOG() << "Init CardWrapper...";
  flx.init(cmd_params);

  TLOG() << "Configure CardWrapper...";
  flx.configure(cmd_params);

  TLOG() << "Start CardWrapper...";
  flx.start(cmd_params);

  TLOG() << "Flipping killswitch in order to stop...";
  if (killswitch.joinable()) {
    killswitch.join();
  }

  TLOG() << "Stop CardWrapper...";
  flx.stop(cmd_params);

  TLOG() << "Stop ElinkHandlers...";
  for (auto const& [tag, handler] : elinks) {
    handler->stop(cmd_params);
  }

  // Filewriter
  std::function<size_t(std::string, std::unique_ptr<LatencyBuffer>&)> write_to_file =
    [&](std::string filename, std::unique_ptr<LatencyBuffer>& buffer) {
      std::ofstream linkfile(filename, std::ios::out | std::ios::binary);
      size_t bytes_written = 0;
      TP_SUPERCHUNK_STRUCT spc;
      while (!buffer->isEmpty()) {
        TLOG() << "chunk length: " << spc.size;
        buffer->read(spc);
        linkfile.write(spc.data.get(), spc.size);
        bytes_written += spc.size;
      }
      return bytes_written;
    };

  TLOG() << "Time to write out the data...";
  std::map<int, std::future<size_t>> done_futures;

  std::ostringstream fnamestr;
  fnamestr << "slr1-" << 5 << "-data.bin";
  TLOG() << "  -> Dropping data to file: " << fnamestr.str();
  std::string fname = fnamestr.str();
  done_futures[5] = std::async(std::launch::async, write_to_file, fname, std::ref(tpbuffer));


  TLOG() << "Wait for them. This might take a while...";
  for (auto& [id, fut] : done_futures) {
    size_t bw = fut.get();
    TLOG() << "[" << id << "] Bytes written: " << bw;
  }

  TLOG() << "GOOD counter: " << good_counter;
  TLOG() << "Total counter: " << total_counter;

  TLOG() << "Number of blocks DMA-d: " << block_counter;

  TLOG() << "Exiting.";
  return 0;
}
