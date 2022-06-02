/**
 * @file FelixCardController.hpp FELIX card controller DAQ Module
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef FLXLIBS_PLUGINS_FELIXCARDCONTROLLER_HPP_
#define FLXLIBS_PLUGINS_FELIXCARDCONTROLLER_HPP_

#include "appfwk/app/Nljs.hpp"
#include "appfwk/cmd/Nljs.hpp"
#include "appfwk/cmd/Structs.hpp"

#include "flxlibs/felixcardcontroller/Structs.hpp"

// From appfwk
#include "appfwk/DAQModule.hpp"

#include "CardControllerWrapper.hpp"

#include <map>
#include <memory>
#include <string>

namespace dunedaq::flxlibs {

class FelixCardController : public dunedaq::appfwk::DAQModule
{
public:
  explicit FelixCardController(const std::string& name);

  FelixCardController(const FelixCardController&) = delete; ///< FelixCardController is not copy-constructible
  FelixCardController& operator=(const FelixCardController&) = delete; ///< FelixCardController is not copy-assignable
  FelixCardController(FelixCardController&&) = delete;            ///< FelixCardController is not move-constructible
  FelixCardController& operator=(FelixCardController&&) = delete; ///< FelixCardController is not move-assignable

  void init(const data_t&) override;

private:
  // Types
  using module_conf_t = dunedaq::flxlibs::felixcardcontroller::Conf;

  // Commands
  void do_configure(const data_t& args);
  void get_info(opmonlib::InfoCollector& ci, int level) override;
  void get_reg(const data_t& args);
  void set_reg(const data_t& args);
  void get_bf(const data_t& args);
  void set_bf(const data_t& args);
  void gth_reset(const data_t& args);

  // Configuration
  module_conf_t m_cfg;

  // FELIX Card
  std::map<uint32_t, std::unique_ptr<CardControllerWrapper> > m_card_wrappers; // NOLINT
};

} // namespace dunedaq::flxlibs

#endif // FLXLIBS_PLUGINS_FELIXCARDCONTROLLER_HPP_
