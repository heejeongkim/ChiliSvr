/*
 * ChiliSvr.h
 *
 *  Created on: Aug 19, 2014
 *      Author: chili
 */

#ifndef CHILISVR_H_
#define CHILISVR_H_

class ChiliSvr{

public:

/*
public:
    QString tmpState;

    //IP ADDRESS
    Q_INVOKABLE char* getIp();

    //To run client thread
    void run() Q_DECL_OVERRIDE;

    //For Publication & Subscription
    Q_INVOKABLE void subscribe(QString topic);
    Q_INVOKABLE void update(QString topic, QString newState);

    //ETC
    char*  s_recv (void *socket);
*/

    ChiliSvr();
    ~ChiliSvr();
};


#endif /* CHILISVR_H_ */
