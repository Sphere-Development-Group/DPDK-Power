#include <rte_common.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_mempool.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

#define BURST_SIZE 16
#define PAYLOAD_SIZE 18

using namespace std;

struct ThreadConfig {
  uint8_t queue_id = 0;
  uint8_t core_id = 0;
  uint8_t port_id = 0;
  bool started = false;
  rte_mbuf** mbuf = nullptr;
  rte_mempool* pool = nullptr;
};

int thread(void* arg);

int main(int argc, char* argv[]) {
  // Получаем число ядер в системе
  string core_mask = "0-" + to_string(sysconf(_SC_NPROCESSORS_CONF) - 1);
  // vector<const char*> eal_config = {"main", "-l", core_mask.c_str(),
  //                                   "--vdev=net_tap1,iface=dpdk-1",
  //                                   "--vdev=net_tap2,iface=dpdk-2"};

  vector<const char*> eal_config = {
      "main",         "-l", core_mask.c_str(), "-a",
      "0000:01:00.0", "-a", "0000:01:00.1"};

  // cout << "Payload size: " << argv[1] << endl;
  cout << "Core mask: " << core_mask << endl;

  if (rte_eal_init(eal_config.size(), const_cast<char**>(eal_config.data())) <
      0)
    rte_exit(rte_errno, "Ошибка инициализации EAL: %s\n",
             rte_strerror(rte_errno));

  // Получение информации о iface
  uint16_t number_interfaces = rte_eth_dev_count_avail();
  cout << "number_interfaces: " << number_interfaces << endl;

  rte_mempool* rx_pool[number_interfaces];
  rte_mempool* tx_pool[number_interfaces];
  rte_eth_conf port_conf[number_interfaces];
  rte_eth_link port_status[number_interfaces];
  rte_eth_stats port_stats[number_interfaces];
  rte_eth_dev_info port_info[number_interfaces];

  // Конфигурация порта и создание RX-пула для него
  for (int port_id = 0; port_id < number_interfaces; port_id++) {
    if (rte_eth_dev_info_get(port_id, &port_info[port_id]) != 0)
      rte_exit(rte_errno, "Ошибка получения HW информации о iface: %s\n",
               rte_strerror(rte_errno));

    //  Пул хранения пакетов на приём
    string rx_pool_name = "rx_pool#" + to_string(port_id);
    rx_pool[port_id] = rte_pktmbuf_pool_create(
        rx_pool_name.c_str(), port_info[port_id].rx_desc_lim.nb_max * 2, 512, 0,
        RTE_MBUF_DEFAULT_DATAROOM + RTE_PKTMBUF_HEADROOM,
        rte_eth_dev_socket_id(port_id));

    // Пул хранения пакетов на отправку
    string tx_pool_name = "tx_pool#" + to_string(port_id);
    tx_pool[port_id] = rte_pktmbuf_pool_create(
        tx_pool_name.c_str(), port_info[port_id].tx_desc_lim.nb_max * 2, 512, 0,
        RTE_MBUF_DEFAULT_DATAROOM + RTE_PKTMBUF_HEADROOM,
        rte_eth_dev_socket_id(port_id));

    port_conf[port_id] = {};  // Очищаем конфигурацию порта

    // RX
    if (port_info[port_id].rx_offload_capa & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM) {
      cout << "Есть поддержка RTE_ETH_RX_OFFLOAD_IPV4_CKSUM" << endl;
      port_conf[port_id].rxmode.offloads |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
    }

    if (port_info[port_id].rx_offload_capa & RTE_ETH_RX_OFFLOAD_UDP_CKSUM) {
      cout << "Есть поддержка RTE_ETH_RX_OFFLOAD_UDP_CKSUM" << endl;
      port_conf[port_id].rxmode.offloads |= RTE_ETH_RX_OFFLOAD_UDP_CKSUM;
    }

    if (port_info[port_id].rx_offload_capa & RTE_ETH_RX_OFFLOAD_TCP_CKSUM) {
      cout << "Есть поддержка RTE_ETH_RX_OFFLOAD_TCP_CKSUM" << endl;
      port_conf[port_id].rxmode.offloads |= RTE_ETH_RX_OFFLOAD_TCP_CKSUM;
    }

    // TX
    if (port_info[port_id].tx_offload_capa & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) {
      cout << "Есть поддержка RTE_ETH_TX_OFFLOAD_IPV4_CKSUM" << endl;
      port_conf[port_id].txmode.offloads |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM;
    }

    if (port_info[port_id].tx_offload_capa & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) {
      cout << "Есть поддержка RTE_ETH_TX_OFFLOAD_UDP_CKSUM" << endl;
      port_conf[port_id].txmode.offloads |= RTE_ETH_TX_OFFLOAD_UDP_CKSUM;
    }

    if (port_info[port_id].tx_offload_capa & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) {
      cout << "Есть поддержка RTE_ETH_TX_OFFLOAD_TCP_CKSUM" << endl;
      port_conf[port_id].txmode.offloads |= RTE_ETH_TX_OFFLOAD_TCP_CKSUM;
    }

    if (rte_eth_dev_configure(port_id, 2, 2, &port_conf[port_id]) < 0)
      rte_exit(rte_errno, "Ошибка конфигурирования интерфейса: %s\n",
               rte_strerror(rte_errno));

    //  RX
    for (int rx_queue = 0; rx_queue < 2; rx_queue++) {
      int rx_queue_cr = rte_eth_rx_queue_setup(
          port_id, rx_queue, port_info[port_id].rx_desc_lim.nb_max,
          rte_eth_dev_socket_id(port_id), NULL, rx_pool[port_id]);
      if (rx_queue_cr != 0)
        rte_exit(rte_errno, "Ошибка настройки RX очереди: %s\n",
                 rte_strerror(rte_errno));
    }
    // TX
    for (int tx_queue = 0; tx_queue < 2; tx_queue++) {
      int tx_queue_cr = rte_eth_tx_queue_setup(
          port_id, tx_queue, port_info[port_id].tx_desc_lim.nb_max,
          rte_eth_dev_socket_id(port_id), NULL);
      if (tx_queue_cr != 0)
        rte_exit(rte_errno, "Ошибка настройки TX очереди: %s\n",
                 rte_strerror(rte_errno));
    }

    // Включение 'неразборчивого' режима
    if (rte_eth_promiscuous_enable(port_id) != 0)
      rte_exit(rte_errno, "Ошибка включение 'неразборчивого' режима: %s\n",
               rte_strerror(rte_errno));

    //  Включение интерфейса
    if (rte_eth_dev_start(port_id) != 0)
      rte_exit(rte_errno, "Ошибка запуска порта: %s\n",
               rte_strerror(rte_errno));

    // Проверка статусов интерфейсов
    if (rte_eth_link_get_nowait(port_id, &port_status[port_id]) != 0)
      rte_exit(rte_errno, "Ошибка получения статуса интерфейса: %s\n",
               rte_strerror(rte_errno));

    cout << "\nPort: " << port_id << endl;
    cout << "Link status: " << port_status[port_id].link_status << endl;
    cout << "Link speed: " << port_status[port_id].link_speed << endl;
  }

  // Реализация простого UDP генератора
  // Шаблоны пакетов под конкретный интерфейс
  rte_mbuf* burst[number_interfaces][BURST_SIZE];
  for (int port_id = 0; port_id < number_interfaces; port_id++) {
    // 1. Выделение памяти под burst[number_interfaces]

    if (rte_pktmbuf_alloc_bulk(tx_pool[port_id], burst[port_id], BURST_SIZE) !=
        0)
      rte_exit(rte_errno,
               "Не удалось создать tx burst[number_interfaces]: %s\n",
               rte_strerror(rte_errno));

    // 2 - Сборка шаблонов (генерация payload + заполнение базовых полей)
    for (int frame_id = 0; frame_id < BURST_SIZE; frame_id++) {
      rte_ether_hdr* l2_hdr = (rte_ether_hdr*)rte_pktmbuf_append(
          burst[port_id][frame_id], sizeof(rte_ether_hdr));

      // L2
      rte_eth_random_addr(l2_hdr->src_addr.addr_bytes);
      rte_eth_random_addr(l2_hdr->dst_addr.addr_bytes);
      l2_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

      // L3
      rte_ipv4_hdr* l3_hdr = (rte_ipv4_hdr*)rte_pktmbuf_append(
          burst[port_id][frame_id], sizeof(rte_ipv4_hdr));
      l3_hdr->version_ihl = RTE_IPV4_VHL_DEF;
      l3_hdr->type_of_service = 0;
      l3_hdr->total_length = 0;
      l3_hdr->packet_id = 0;
      l3_hdr->fragment_offset = 0;
      l3_hdr->time_to_live = 64;
      l3_hdr->next_proto_id = IPPROTO_UDP;
      l3_hdr->src_addr = rte_cpu_to_be_32(RTE_IPV4(10, 0, 0, 1));
      l3_hdr->dst_addr = rte_cpu_to_be_32(RTE_IPV4(10, 0, 0, 2));
      l3_hdr->hdr_checksum = 0;  // Используется HW расчёт чексум

      // L4
      rte_udp_hdr* udp = (rte_udp_hdr*)rte_pktmbuf_append(
          burst[port_id][frame_id], sizeof(rte_udp_hdr));
      udp->src_port = rte_cpu_to_be_16(49100);
      udp->dst_port = rte_cpu_to_be_16(80);
      udp->dgram_len = 0;
      udp->dgram_cksum = 0;  // HW расчёт чексуммы

      // Payload
      uint32_t* payload =
          (uint32_t*)rte_pktmbuf_append(burst[port_id][frame_id], PAYLOAD_SIZE);
      uint32_t flow_hash = rte_hash_crc(l3_hdr, sizeof(rte_ipv4_hdr), 0);
      payload[0] = flow_hash;
      memset(&payload[1], 0x00, 1472 - sizeof(uint32_t));

      l3_hdr->total_length = rte_cpu_to_be_16(
          sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr) + PAYLOAD_SIZE);
      udp->dgram_len = rte_cpu_to_be_16(sizeof(rte_udp_hdr) + PAYLOAD_SIZE);

      burst[port_id][frame_id]->l2_len = sizeof(rte_ether_hdr);
      burst[port_id][frame_id]->l3_len = sizeof(rte_ipv4_hdr);
      burst[port_id][frame_id]->l4_len = sizeof(rte_udp_hdr);
      burst[port_id][frame_id]->ol_flags |=
          RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_UDP_CKSUM;
    }
  }

  ThreadConfig config[number_interfaces];
  for (int port_id = 0; port_id < number_interfaces; port_id++) {
    config[port_id].core_id = port_id + 1;
    config[port_id].port_id = 0;
    config[port_id].queue_id = port_id;
    config[port_id].mbuf = burst[port_id];
    config[port_id].pool = tx_pool[port_id];

    if (rte_eal_remote_launch(thread, &config[port_id], 1 + port_id) != 0)
      rte_exit(rte_errno, "Ошибка запуска потока: %s\n",
               rte_strerror(rte_errno));
  }

  while (true) {
    string cli_command;
    cout << "\nstat - Статистика по интерфейсам + PPS\nstart - Запуск "
            "генерации\nstop - Остановка генерации"
         << endl;
    cout << "Command: ";
    cin >> cli_command;
    if (cli_command == "stat") {
      double tx_mbps;
      double rx_mbps;

      uint64_t opackets = 0;  // Число отправленных пакетов
      uint64_t ipackets = 0;  // Число принятых пакетов

      for (uint port_id = 0; port_id < number_interfaces; port_id++) {
        rte_eth_stats_get(port_id, &port_stats[port_id]);
        opackets = port_stats[port_id].opackets;
        ipackets = port_stats[port_id].ipackets;
        rte_delay_ms(1000);
        rte_eth_stats_get(port_id, &port_stats[port_id]);
        opackets = port_stats[port_id].opackets - opackets;
        ipackets = port_stats[port_id].ipackets - ipackets;

        tx_mbps = ((PAYLOAD_SIZE + 38 + 28) * 8) * opackets / (1024 * 1024);
        rx_mbps = ((PAYLOAD_SIZE + 38 + 28) * 8) * ipackets / (1024 * 1024);

        cout << "\n==========================================" << endl;
        cout << "Port ID: " << port_id << endl;
        cout << "TX PPS: " << opackets << endl;
        cout << "TX MBS: " << tx_mbps << endl;
        cout << "\nRX PPS: " << ipackets << endl;
        cout << "RX MBS: " << rx_mbps << endl;
        cout << "==========================================" << endl;
      }
    }
    if (cli_command == "start")
      for (uint port_id = 0; port_id < number_interfaces; port_id++)
        config[port_id].started = true;

    if (cli_command == "stop")
      for (uint port_id = 0; port_id < number_interfaces; port_id++)
        config[port_id].started = false;
  }
  rte_eal_cleanup();
  return 0;
}

int thread(void* arg) {
  rte_mbuf* burst[BURST_SIZE];
  ThreadConfig* config = (ThreadConfig*)arg;
  cout << "Поток запущен на ядре: " << config->core_id << endl;

  // Копирование шаблона в память локальную память потока
  while (true) {
    if (config->started == true) {
      for (int frame_id = 0; frame_id < BURST_SIZE; frame_id++)
        burst[frame_id] =
            rte_pktmbuf_clone(config->mbuf[frame_id], config->pool);

      uint16_t sent_frames = rte_eth_tx_burst(config->port_id, config->queue_id,
                                              burst, BURST_SIZE);

      if (unlikely(sent_frames < BURST_SIZE))
        for (uint frame_id = sent_frames; frame_id < BURST_SIZE; frame_id++)
          rte_pktmbuf_free(burst[frame_id]);
    }
  }
  return rte_errno;
}