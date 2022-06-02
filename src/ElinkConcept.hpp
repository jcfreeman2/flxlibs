/**
 * @file ElinkConcept.hpp ElinkConcept for constructors and
 * forwarding command args. Enforces the implementation to
 * queue in block_addresses
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FLXLIBS_SRC_ELINKCONCEPT_HPP_
#define FLXLIBS_SRC_ELINKCONCEPT_HPP_

#include "DefaultParserImpl.hpp"

#include "appfwk/DAQModule.hpp"
#include "packetformat/detail/block_parser.hpp"
#include <nlohmann/json.hpp>

#include <memory>
#include <sstream>
#include <string>

namespace dunedaq::flxlibs {

class ElinkConcept
{
public:
  ElinkConcept()
    : m_parser_impl()
    , m_elink_str("")
    , m_elink_source_tid("")
  {
    m_parser = std::make_unique<felix::packetformat::BlockParser<DefaultParserImpl>>(m_parser_impl);
  }
  virtual ~ElinkConcept() {};

  ElinkConcept(const ElinkConcept&) = delete;            ///< ElinkConcept is not copy-constructible
  ElinkConcept& operator=(const ElinkConcept&) = delete; ///< ElinkConcept is not copy-assginable
  ElinkConcept(ElinkConcept&&) = delete;                 ///< ElinkConcept is not move-constructible
  ElinkConcept& operator=(ElinkConcept&&) = delete;      ///< ElinkConcept is not move-assignable

  virtual void init(const nlohmann::json& args, const size_t block_queue_capacity) = 0;
  virtual void set_sink(const std::string& sink_name) = 0;
  virtual void conf(const nlohmann::json& args, size_t block_size, bool is_32b_trailers) = 0;
  virtual void start(const nlohmann::json& args) = 0;
  virtual void stop(const nlohmann::json& args) = 0;
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;

  virtual bool queue_in_block_address(uint64_t block_addr) = 0; // NOLINT

  DefaultParserImpl& get_parser() { return std::ref(m_parser_impl); }

  void set_ids(int card, int slr, int id, int tag)
  {
    m_card_id = card;
    m_logical_unit = slr;
    m_link_id = id;
    m_link_tag = tag;

    std::ostringstream lidstrs;
    lidstrs << "Elink["
            << "cid:" << std::to_string(m_card_id) << "|"
            << "slr:" << std::to_string(m_logical_unit) << "|"
            << "lid:" << std::to_string(m_link_id) << "|"
            << "tag:" << std::to_string(m_link_tag) << "]";
    m_elink_str = lidstrs.str();

    std::ostringstream tidstrs;
    tidstrs << "ept-" << std::to_string(m_card_id) << "-" << std::to_string(m_logical_unit);
    m_elink_source_tid = tidstrs.str();

    m_opmon_str =
      "elink_" + std::to_string(m_card_id) + "_" + std::to_string(m_logical_unit) + "_" + std::to_string(m_link_id);
  }

protected:
  // Block Parser
  DefaultParserImpl m_parser_impl;
  std::unique_ptr<felix::packetformat::BlockParser<DefaultParserImpl>> m_parser;

  int m_card_id = 0;
  int m_logical_unit = 0;
  int m_link_id = 0;
  int m_link_tag = 0;
  std::string m_elink_str;
  std::string m_opmon_str;
  std::string m_elink_source_tid;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_t0;

private:
};

} // namespace dunedaq::flxlibs

#endif // FLXLIBS_SRC_ELINKCONCEPT_HPP_
