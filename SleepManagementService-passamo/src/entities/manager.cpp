#include "manager.h"
#include <time.h>

extern int PORT_SERVER;
extern int PORT_CLI;

//=======================================================================
void ManagerEntity::run()

{
  rec_port = PORT_SERVER;
  send_port = PORT_CLI;
  update = true;
  bNeedsElection = true;
  bReceivedElectionAnswer = false;
  bIsLeader = true;
  status.id = 1;

  //std::thread election(&ManagerEntity::handleElectionThread, this);
  std::thread sendDiscoveryThread(&ManagerEntity::handleDiscoveryThread, this);
  std::thread sendMonitoringThread(&ManagerEntity::handleMonitoringThread, this);
  std::thread managementThread(&ManagerEntity::handleManagementThread, this);
  std::thread interfaceThread(&ManagerEntity::handleInterfaceThread, this); 
  //std::thread receiveMessagesThread(&ManagerEntity::handleReceiveThread, this);
  std::thread messagesThread(&ManagerEntity::handleMessageThread, this);
  std::thread IOThread(&ManagerEntity::handleIOThread, this);
  std::thread replicationThread(&ManagerEntity::handleReplicationThread, this);

  //election.join();
  sendDiscoveryThread.join();
  sendMonitoringThread.join();
  interfaceThread.join();
  managementThread.join();
  // receiveMessagesThread.join();
  messagesThread.join();
  IOThread.join();
  replicationThread.join();
}

void ManagerEntity::handleElectionThread(){
  while(1){
    if(bNeedsElection){
      MessageManager messageManager;
      messageManager.setSocket(send_port, true);


          PACKET packet;
          packet.type = ELECTION_PACKET;
          strcpy(packet.ip_addr, status.ip_addr);
          strcpy(packet.hostname, status.name);
	  strcpy(packet.mac_addr, status.mac_addr);
	  packet.active = true;
	  packet.awake = false;
          packet.processId = status.id;
    	  packet.nParticipants = 0;
          //printf("id1 %d\n", status.id);
	  //printf("id2 %d\n", packet.processId);
          
	  //printf("id %d\n", packet.processId);

          messageManager.sendMessage(packet);
        
 

      messageManager.closeSocket();

      time_t timeSent = clock();
      while((double)(clock() - timeSent)/CLOCKS_PER_SEC < 5){
        if(bReceivedElectionAnswer){
          bIsLeader = false;
          break;
        }
      }
      if(!bReceivedElectionAnswer){
        bIsLeader = true;
      }

      bReceivedElectionAnswer = false;
    }
    sleep(1);
  }
}

//=======================================================================
void ManagerEntity::handleMessageThread()
{
  PACKET packet;
  PARTICIPANT p;
  MessageManager messageManager; 
  struct sockaddr_in rep_addr;
  messageManager.setSocket(rec_port, false);
  std::map<std::string, PARTICIPANT>::iterator it;

  while (true){
    messageManager.receiveMessage(&rep_addr, &packet);
    rep_addr.sin_port = htons(PORT_SERVER);

    status_mutex.lock();

    gethostname(packet.hostname, MSG_STR_LEN);
    messageManager.getIpAddress(packet.ip_addr); 
    getMacAddress(packet.mac_addr);
    strcpy(status.name, packet.hostname);
    strcpy(status.ip_addr, packet.ip_addr);
    strcpy(status.mac_addr, packet.mac_addr);

    printf("%d\n", packet.type);

    switch (packet.type){

      case SLEEP_DISCOVERY_PACKET:
        packet.active = status.active;
        packet.type = REPLY_SLEEP_DISCOVERY_PACKET;
	strcpy(packet.hostname, status.name);
        //if (status.active && status.awake)
        messageManager.replyMessage(rep_addr, packet);
	printf("NAOFUUDEUUU\n");
        break;

      case REPLY_SLEEP_DISCOVERY_PACKET:   
        if (pMap.count(packet.hostname)) {
          it = pMap.find(packet.hostname);
          it->second.discovery_count = 0;
          it->second.active = true;
        }
        else{
          strcpy(p.name, packet.hostname);
          strcpy(p.ip_addr, packet.ip_addr);
          strcpy(p.mac_addr, packet.mac_addr);
          p.active = true;
          p.awake = packet.awake;
          p.monitoring_count = 0;   
          p.discovery_count = 0;
          pMap.insert(std::pair<std::string, PARTICIPANT>(packet.hostname, p));
          printf("Here2\n");
          printf("size pmap: %d\n", pMap.size()); 
        }
        break;
        
      case SLEEP_MONITORING_PACKET:
        packet.awake = status.awake;
        packet.type = REPLY_SLEEP_MONITORING_PACKET;
        //if (status.active && status.awake)
        messageManager.replyMessage(rep_addr, packet);
        break;

      case REPLY_SLEEP_MONITORING_PACKET:
        if (pMap.count(packet.hostname)){
          it = pMap.find(packet.hostname);
          it->second.discovery_count = 0;
          it->second.monitoring_count = 0;
          it->second.awake = packet.awake;
        }
        break;

      case WAKEUP_PACKET:
        status.awake = packet.awake;
        break;
      
      case ELECTION_PACKET:
        packet.type = ANSWER_PACKET;
	printf("id1 receive %d\n", packet.processId);
	printf("id2 receive %d\n", status.id);
	printf("receive %s\n", packet.hostname);
	if(packet.processId < status.id){
		messageManager.replyMessage(rep_addr, packet);
		bNeedsElection = true;
}
        break;

      case ANSWER_PACKET:
        bReceivedElectionAnswer = true;
        break;

      case EXIT_PACKET:   
        if (pMap.count(packet.hostname)){
          it = pMap.find(packet.hostname); 
          it->second.active = false;
          it->second.awake = false;
        }  
        break;

      case REPLICATION_PACKET:  
       
        pMap_mutex.lock();
        pMap.clear();
        for(int i = 0; i < packet.nParticipants; i++){
          PARTICIPANT p;
          strcpy(p.name, packet.participants[i].name);
          strcpy(p.ip_addr, packet.participants[i].ip_addr);
          strcpy(p.mac_addr, packet.participants[i].mac_addr);
          p.active = packet.participants[i].active;
          p.awake = packet.participants[i].awake;
          p.monitoring_count = packet.participants[i].monitoring_count;   
          p.discovery_count = packet.participants[i].discovery_count;
          pMap.insert(std::pair<std::string, PARTICIPANT>(packet.hostname, p));
        }  
        pMap_mutex.unlock();
        break;

      default : fprintf(stderr, "Wrong packet: %d\n", packet.type);
    }

    status_mutex.unlock();
  }

  messageManager.closeSocket();
}

//=======================================================================
// void ManagerEntity::handleReceiveThread(){
//   struct sockaddr_in rep_addr;
//   PACKET packet;
//   PARTICIPANT p;
//   std::map<std::string, PARTICIPANT>::iterator it;
//   MessageManager messageManager; 
//   messageManager.setSocket(rec_port, false);

//   while (1){
//     messageManager.receiveMessage(&rep_addr, &packet);
    
//     pMap_mutex.lock();

//     switch (packet.type){


//       default : fprintf(stderr, "Wrong packet: %d\n", packet.type);
//     }

//     pMap_mutex.unlock();
//   }

//   messageManager.closeSocket();
// }

//=======================================================================
void ManagerEntity::handleManagementThread(){
  while(1){
    if(bIsLeader){
      pMap_mutex.lock();
      for (auto it = pMap.begin(); it != pMap.end(); ++it){
        if (it->second.monitoring_count >= MAX_MONITORING_COUNT){
          it->second.awake = false;
        }
      }
      pMap_mutex.unlock();
    }
    sleep(1);
  }
}

//=======================================================================
void ManagerEntity::handleMonitoringThread(){
  PACKET packet;
  packet.type = SLEEP_MONITORING_PACKET;
  packet.awake = false;
  packet.active = false;

  repeatMessage(packet, 1);
}

//=======================================================================
void ManagerEntity::handleDiscoveryThread(){
  PACKET packet;
  packet.type = SLEEP_DISCOVERY_PACKET;
  packet.awake = false;
  packet.active = false;

  repeatMessage(packet, 1);
}

//=======================================================================
void ManagerEntity::incrementCounters(int type){
  pMap_mutex.lock();
  for (auto it = pMap.begin(); it != pMap.end(); ++it){
    switch(type){
    case SLEEP_DISCOVERY_PACKET: 
      it->second.discovery_count++;
      break;

    case SLEEP_MONITORING_PACKET: 
      it->second.monitoring_count++;
      break;
    }
  }
  pMap_mutex.unlock();
}

//=======================================================================
void ManagerEntity::handleIOThread(){
  char command[10];
  char hostname[MSG_STR_LEN];

  while(true){
    char str[10];
    scanf("%s", str);

    if (strcmp(str, "c") == 0){
      update = false;
      fprintf(stderr, "\033[A\bType command: ");
      scanf(" %s %s", command, hostname);
    
      if ((strcmp(command, "WAKEUP") == 0) && bIsLeader){
        std::map<std::string, PARTICIPANT>::iterator it; 
       
        pMap_mutex.lock();
        it = pMap.find(hostname);
        pMap_mutex.unlock();

        PACKET packet;
        packet.type = WAKEUP_PACKET;
        strcpy(packet.hostname, it->second.name);
        strcpy(packet.ip_addr, it->second.ip_addr);
        strcpy(packet.mac_addr, it->second.mac_addr);
        packet.awake = true;

        char command[30];

        sprintf(command, "wakeonlan %s", packet.mac_addr);
        system(command);
      }
      else if (strcmp(command, "exit") == 0)
        terminate(); 

      update = true;
    }
  }
}

//=======================================================================
void ManagerEntity::repeatMessage(PACKET packet, float interval){
  MessageManager messageManager; 
  messageManager.setSocket(send_port, true);

  while(true){   

    if(bIsLeader){
      if(packet.type == REPLICATION_PACKET){
        
        int i = 0;
        for (auto it = pMap.begin(); it != pMap.end(); ++it, i++){
          if(i < MAX_PARTICIPANTS){
            packet.participants[i] = it->second;
          }
        }
        packet.nParticipants = i;
	//printf("%d participantes\n", packet.nParticipants);
        messageManager.sendMessage(packet);
      }
      else{
        incrementCounters(packet.type);
        messageManager.sendMessage(packet);
      }
    }   
    sleep(interval);
  }

  messageManager.closeSocket();
}

//=======================================================================
void ManagerEntity::sendMessage(PACKET packet){
  MessageManager messageManager; 
  messageManager.setSocket(send_port, true);
  messageManager.sendMessage(packet);
  messageManager.closeSocket();
}

//=======================================================================
void ManagerEntity::handleInterfaceThread(){
  while(true){
    if (update){
      std::cout << "\033[H\033[2J\033[3J" ;
      if(bIsLeader){
      	std::cout << "MANAGER\n\n";
      }
      else{
	std::cout << "CLIENT\n\n";
      }

      TextTable *table = new TextTable('-', '|', '+');

      table->add( "Hostname" );
      table->add( "MAC Address" );
      table->add( "IP Address" );
      table->add( "Status" );
      // table->add( "Disc_Count" );
      table->add( "Mon_Count" );
      table->endOfRow();

      pMap_mutex.lock();
      for (auto it = pMap.begin(); it != pMap.end(); ++it){
        if (it->second.active){
          table->add(it->second.name);
          table->add(it->second.mac_addr);
          table->add(it->second.ip_addr);
          table->add(it->second.awake ? "awake" : "ASLEEP" );
          // table->add(std::to_string(it->second.discovery_count));
          table->add(std::to_string(it->second.monitoring_count));
          table->endOfRow();
        }
      }
      pMap_mutex.unlock();

      table->setAlignment( 2, TextTable::Alignment::RIGHT );
      std::cout << *table;
      delete table;
      sleep(2);
    }
  }
}

//=======================================================================
void ManagerEntity::getMacAddress(char* mac_addr){
  std::string str_mac;
  std::ifstream in(MAC_ADDR_PATH);
  std::getline(in, str_mac);

  strcpy(mac_addr, str_mac.c_str());
}

//=======================================================================
void ManagerEntity::terminate()
{
  status.active = false;
  PACKET packet; 
  MessageManager messageManager; 
  messageManager.setSocket(PORT_SERVER, true);

  packet.type = EXIT_PACKET;
  packet.active = false;
  packet.awake = false;
  strcpy(packet.hostname, status.name);
  strcpy(packet.ip_addr, status.ip_addr);
  strcpy(packet.mac_addr, status.mac_addr);

  messageManager.sendMessage(packet);
  messageManager.closeSocket();

  exit(1);
}

//=======================================================================
/*
void ManagerEntity::handleReplicationThread(){
  while(1){
    if(bNeedsElection){
      MessageManager messageManager;
      messageManager.setSocket(send_port, false);

      pMap_mutex.lock();
      for (auto it = pMap.begin(); it != pMap.end(); ++it){
        if(it->second.id > this->status.id){
          struct sockaddr_in address;
          address.sin_family = AF_INET;
          address.sin_addr.s_addr = inet_addr(it->second.ip_addr);
          address.sin_port = htons(send_port);

          PACKET packet;
          packet.type = ELECTION_PACKET;
          strcpy(packet.ip_addr, status.ip_addr);
          strcpy(packet.hostname, status.name);

          messageManager.replyMessage(address, packet);
        }
      }
      pMap_mutex.unlock();
      messageManager.closeSocket();

      time_t timeSent = clock();
      while((double)(clock() - timeSent)/CLOCKS_PER_SEC < 5){
        if(bReceivedElectionAnswer){
          bIsLeader = false;
          break;
        }
      }
      if(!bReceivedElectionAnswer){
        bIsLeader = true;
      }

      bReceivedElectionAnswer = false;
    }
  }
}
*/

void ManagerEntity::handleReplicationThread(){

  PACKET packet;
  packet.type = REPLICATION_PACKET;
  packet.awake = false;
  packet.active = false;

  repeatMessage(packet, 5);

}
