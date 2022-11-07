/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd. All rights reserved.
 *
 */

#ifndef SA_SERVER_DISPATCHER_H
#define SA_SERVER_DISPATCHER_H

#include "msg/Dispatcher.h"
#include "msg/Messenger.h"

#include "msg_module.h"

class NetworkModule;
class SaServerDispatcher : public Dispatcher {
    bool active { false };
    Messenger *messenger { nullptr };
    uint64_t dcount { 0 };
    MsgModule *ptrMsgModule { nullptr };
    NetworkModule *ptrNetworkModule { nullptr };

public:
   SaServerDispatcher() = delete;
   explicit SaServerDispatcher( Messenger *msgr, MsgModule *msgModule, NetworkModule *networkModule);
   ~SaServerDispatcher() override;

   uint64_t get_dcount()
   {
       return dcount;
   }
   void set_active()
   {
	active = true;
   }

   bool ms_dispatch(Message *m) override;
   void ms_handle_connect(Connection *con) override {};
   void ms_handle_accept(Connection *con) override {};
   bool ms_handle_reset(Connection *con) override;
   void ms_handle_remote_reset(Connection *con) override;
   
   bool ms_handle_refused(Connection *con) override
   {
	return false;
   }
   bool ms_get_authorizer(int dest_type, AuthAuthorizer **a) override
   {
	return false;
   };
  
   int ms_handle_authentication(Connection *con) override
   {
	return 1;
   }
};

#endif

