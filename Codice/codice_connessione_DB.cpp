#include <cstdio>
#include <iostream>
#include <string>
#include "dependencies/include/libpq-fe.h"
using std::cout;
using std::endl;
using std::string;
using std::cin;

void printLine(int campi, int* maxChar) {
    for (int j = 0; j < campi; ++j) {
        cout << '|';
        for (int k = 0; k < maxChar[j] + 2; ++k)
            cout << '-';
    }
    cout << "|\n";
}

void printQuery(PGresult* res) {
    // Preparazione dati
    const int tuple = PQntuples(res), campi = PQnfields(res);
    string v[tuple + 1][campi];

    for (int i = 0; i < campi; ++i) {
        string s = PQfname(res, i);
        v[0][i] = s;
    }
    
    for (int i = 0; i < tuple; ++i)
        for (int j = 0; j < campi; ++j) {
            if (string(PQgetvalue(res, i, j)) == "t" || string(PQgetvalue(res, i, j)) == "f")
                if (string(PQgetvalue(res, i, j)) == "t")
                    v[i + 1][j] = "si";
                else
                    v[i + 1][j] = "no";
            else
                v[i + 1][j] = PQgetvalue(res, i, j);
        }
    
    int maxChar[campi];
    for (int i = 0; i < campi; ++i)
        maxChar[i] = 0;

    for (int i = 0; i < campi; ++i) {
        for (int j = 0; j < tuple + 1; ++j) {
            int size = v[j][i].size();
            maxChar[i] = size > maxChar[i] ? size : maxChar[i];
        }
    }

    // Stampa effettiva delle tuple
    printLine(campi, maxChar);
    for (int j = 0; j < campi; ++j) {
        cout << "| ";
        cout << v[0][j];
        for (int k = 0; k < maxChar[j] - v[0][j].size() + 1; ++k)
            cout << ' ';
        if (j == campi - 1)
            cout << "|";
    }
    cout << endl;
    printLine(campi, maxChar);

    for (int i = 1; i < tuple + 1; ++i) {
        for (int j = 0; j < campi; ++j) {
            cout << "| ";
            cout << v[i][j];
            for (int k = 0; k < maxChar[j] - v[i][j].size() + 1; ++k)
                cout << ' ';
            if (j == campi - 1)
                cout << "|";
        }
        cout << endl;
    }
    printLine(campi, maxChar);
}

PGconn* connect(const char* host, const char* user, const char* db, const char* pass, const char* port) {
    char conninfo[256];
    sprintf(conninfo, "user=%s password=%s dbname=\'%s\' hostaddr=%s port=%s",
        user, pass, db, host, port);

    PGconn* conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Errore di connessione" << endl << PQerrorMessage(conn);
        PQfinish(conn);
        exit(1);
    }else{
        std::cout <<endl;
        std::cout << "Connessione effettuata al DB di Monitoraggio Pazienti by M&M!" << endl;
    }

    return conn;
}

PGresult* execute(PGconn* conn, const char* query) {
    PGresult* res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        cout << " Risultati sbagliati!" << PQerrorMessage(conn) << endl;
        PQclear(res);
        exit(1);
    }

    return res;
}

char* chooseParam(PGconn* conn, const char* query, const char* table) {
    PGresult* res = execute(conn, query);
    printQuery(res);

    const int tuple = PQntuples(res), campi = PQnfields(res);
    int val;
    cout<< "Inserisci numero compreso tra 1 e "<<tuple<<" per selezionare un numero per la media: "<<endl;
    cin >> val;
    while (val <= 0 || val > tuple) {
        cout << "Valore non valido\n";
        cout << "Inserisci un numero corretto nella tabella " << table << " scelto: ";
        cin >> val;
    }
    return PQgetvalue(res, val - 1, 0);
}

int main(int argc, char** argv) {
    cout << "Inserisci la password del tuo DB: ";
    char pass[50];
    cin >> pass;
    PGconn* conn = connect("127.0.0.1", "postgres", "Monitoring", pass, "5432");

    const char* query[6] = {
        "SELECT Misurazione.Codice as CodiceMisurazione, Sp02, Pi, FR, FC, Misurazione.Data as DataMisurazione\
        FROM Dati_personali, Misurazione, Cartella_clinica \
        WHERE Dati_personali.Cartella = Cartella_clinica.Codice AND Misurazione.Cartella = Cartella_clinica.Codice AND Dati_personali.Nome='%s' AND Dati_personali.Cognome= '%s' AND Data >= to_timestamp('%s')",

        "DROP VIEW IF EXISTS PazientiRegione;\
        CREATE VIEW PazientiRegione AS SELECT Pa.CF, Dp.Nome as NomePaziente, Dp.Cognome as CognomePaziente, Str.Nome as NomeStruttura, Str.Tipologia as TipologiaStruttura \
        FROM Struttura_sanitaria as Str,Paziente as Pa , Indirizzo as Ind , Dati_personali as Dp , Cartella_clinica as Cc\
        WHERE Pa.Struttura = Str.Id AND Ind.Struttura = Str.Id AND Pa.Cartella = Cc.Codice AND Dp.Cartella = Cc.Codice AND Ind.Regione = '%s' ;\
        SELECT PazientiRegione.NomeStruttura, PazientiRegione.TipologiaStruttura, count(*) as NumeroPazienti\
        FROM PazientiRegione\
        GROUP BY PazientiRegione.NomeStruttura, PazientiRegione.TipologiaStruttura;",


        "SELECT TrasfTotCP.NomeStruttura, TrasfTotCp.TrasferimentiTotali\
        FROM Indirizzo as Ind , Struttura_sanitaria as Stru ,(SELECT Str.Nome as NomeStruttura, count(Trasf.Codice) as TrasferimentiTotali \
			FROM Trasferimento as Trasf, Struttura_sanitaria as Str WHERE Trasf.Struttura = Str.Id AND Str.Tipologia = 'Centro pneumologico' GROUP BY Str.Nome) TrasfTotCP \
        WHERE Ind.Struttura=Stru.Id AND Stru.Nome=TrasfTotCP.NomeStruttura AND Ind.Regione = '%s';",

        "DROP VIEW IF EXISTS NumeroPazientiRicoveratiPerRegione;\
        CREATE VIEW NumeroPazientiRicoveratiPerRegione AS\
        SELECT Ind.Regione , count(*) as NumeroPazientiRicoverati\
        FROM Indirizzo as Ind, Struttura_sanitaria as Str , Paziente as Pa \
        WHERE Ind.Struttura = Str.Id AND Pa.Struttura = Str.Id AND Pa.Ricoverato = 'TRUE'\
        GROUP BY Ind.Regione;\
        SELECT round(avg(NPRPR.NumeroPazientiRicoverati)) as MediaTotale\
        FROM NumeroPazientiRicoveratiPerRegione as NPRPR;",

        "SELECT Dp.Nome , Dp.Cognome , Pa.CF , MediaMisurazioniPaziente.Media as MediaSp02\
        FROM Paziente as Pa,Cartella_clinica as Cc , Dati_personali as Dp ,Caratteristiche_fisico_sanitarie as CFS , (SELECT Paz.CF, round(avg(Misu.Sp02)) as Media\
            FROM Paziente as Paz, Misurazione as Misu WHERE Misu.CodPaziente = Paz.CF GROUP BY Paz.CF HAVING round(avg(Misu.Sp02)) <= %s) MediaMisurazioniPaziente \
        WHERE Pa.Cartella = Cc.Codice AND Dp.Cartella = Cc.Codice AND CFS.Cartella = Cc.Codice AND CFS.Sesso = '%s' AND MediaMisurazioniPaziente.CF = Pa.CF;"
    };

    while (true) {
        cout << endl;
        cout << "1) Visualizzare le misurazioni del paziente desiderato con data successiva all'anno inserito\n";
        cout << "2) Visualizzare il numero di tutti i pazienti ricoverati nelle strutture sanitarie di una regione selezionata raggruppati per struttura e per tipologia di struttura\n";
        cout << "3) Visualizzare il numero dei trasferimenti presso centri pneumologici collocati nella regione selezionata\n";
        cout << "4) Calcolare la media del numero totale di pazienti ricoverati nelle strutture sanitarie nazionali\n";
        cout << "5) Visualizzare i dati e la media del valore di Sp02 dei pazienti del sesso selezionato aventi una media di Sp02 <= al valore selezionato \n";
        cout << "Query da eseguire (0 per uscire): ";

        int q = 0;
        cin >> q;
        system("CLS");
        while (q < 0 || q > 5) {
            cout << "Le query vanno da 1 a 5...\n";
            cout << "Query da eseguire (0 per uscire): ";
            cin >> q;
        }
        if (q == 0) break;
        char queryTemp[1500];

        int i = 0;
        switch (q) {
        case 1:
            char nome[30];
            char cognome[30];
            char anno[5];
            cout << "Inserisci il nome: ";
            cin>>nome;
            cout << "Inserisci il cognome: ";
            cin>>cognome;
            cout<< "Inserisci l'anno: ";
            cin>>anno;
            cout<<endl;

            sprintf(queryTemp,query[0],nome,cognome,anno);
            printQuery(execute(conn,queryTemp));
            break;
        case 2:
            char regione1[30];
            cout << "Inserisci la regione: ";
            cin >> regione1;
            cout<<endl;
            sprintf(queryTemp, query[1], regione1);
            printQuery(execute(conn, queryTemp));
            break;
        case 3:
            char regione2[30];
            cout << "Inserisci la regione: ";
            cin >> regione2;
            cout<<endl;
            sprintf(queryTemp, query[2], regione2);
            printQuery(execute(conn, queryTemp));
            break;
        case 5:
        {
            char sesso[1];
            cout<< "Inserisci il sesso: ";
            cin>>sesso;
            cout<<endl;

            sprintf(queryTemp, query[4],chooseParam(conn,"SELECT MediaMisurazioniPaziente.Media as MedieEsistenti\
                FROM Paziente as Pa,Cartella_clinica as Cc , Dati_personali as Dp ,Caratteristiche_fisico_sanitarie as CFS , (SELECT Paz.CF, round(avg(Misu.Sp02)) as Media\
                FROM Paziente as Paz, Misurazione as Misu WHERE Misu.CodPaziente = Paz.CF GROUP BY Paz.CF HAVING round(avg(Misu.Sp02)) < 100) MediaMisurazioniPaziente \
                WHERE Pa.Cartella = Cc.Codice AND Dp.Cartella = Cc.Codice AND CFS.Cartella = Cc.Codice AND MediaMisurazioniPaziente.CF = Pa.CF GROUP BY MediaMisurazioniPaziente.Media order by MediaMisurazioniPaziente.Media asc;","MedieEsistenti"),sesso);
            printQuery(execute(conn, queryTemp));
            break;
        }
        default:
            printQuery(execute(conn, query[q - 1]));
            break;
        }
        system("pause");   
    }

    PQfinish(conn);
}
