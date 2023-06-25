/*
bot.cpp
dzbot
6/20/2023
speedyplane
*/
#include "discord.h"
#include "pldb.h"
//#include "vacman.h"
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>

Client* DzClient;
Base* DzBase;

void OnClientReady(User* DzUser);
void OnMessageCreate(Message* DzMessage);
void OnMessageUpdate(Message* DzMessage);
void OnGuildMemberAdd(Member* DzMember);
void OnSteamBan(const char* ProfileLink);

/*
vacman stuff
*/

int main(int argc, char** argv) {
	DzBase = new Base("dzdb.plb");
	if (!DzBase) {
		printf("Could not obtain file lock on database.");
		return -1;
	}
	if (!DzBase->GetTable("DzCheaters")) {
		Table* Cheaters = DzBase->CreateTable("DzCheaters");
		Field* PrimKey = Cheaters->CreateField("CSteamLink", TypeStr, 240);
		Cheaters->CreateField("CReportCnt", TypeLongInt, 4);
		Cheaters->SetFieldAttributes(PrimKey, AttribPrimaryKey, true);
	}
	if (!DzBase->GetTable("DzInfractions")) {
		Table* Infractions = DzBase->CreateTable("DzInfractions");
		Field* PrimKey = Infractions->CreateField("IInfractionId", TypeLongLongInt, 8);
		Infractions->CreateField("IDiscordId", TypeLongLongInt, 8);
		Infractions->CreateField("IReason", TypeStr, 256);
		Infractions->SetFieldAttributes(PrimKey, AttribPrimaryKey, true);
		Infractions->SetFieldAttributes(PrimKey, AttribAutoIncrement, true);
	}
	
	DzClient = new Client;

	DzClient->Register(ClientReady, &OnClientReady);
	DzClient->Register(MessageCreate, &OnMessageCreate);
	DzClient->Register(MessageUpdate, &OnMessageUpdate);
	DzClient->Register(GuildMemberAdd, &OnGuildMemberAdd);

	// pldb -> vacman database for the watcher


	DzClient->Login("MTEyMDg2MTIzMjkxNjg3NzM0Mg.GgnXwx.vm5TJrNj-iI0glrDzpGckOBCJY_tyhqW-KNVYQ");

	while (1);
}

void Reprimand(u64 User, const char* Why) {
	Channel* The1984 = DzClient->Channels.At(1121679676763557990);
	Table* Reprimands = DzBase->GetTable("DzInfractions");
	u64 NextKey = Reprimands->AutoKey();
	Entry* NextEntry = Reprimands->CreateEntry(&NextKey);
	NextEntry->Update<u64>("IDiscordId", &User);
	NextEntry->Update<const char*>("IReason", &Why);

	int InfractionCount = Reprimands->TableSearch<int>("SELECT COUNT(*) WHERE IDiscordId = %llu;", User);	

	Member* TheMember = DzClient->Guild.At(216726067770163200)->Members.At(User);
	if (!TheMember) {
		// ??
		return;
	}

	if (TheMember->Roles.At(1120853731592908820)) { // STAFF
		The1984->Send("A negative action was performed by %s, Reason being: %s. This was infraction #%i for them.", TheMember->UserPtr->DisplayName, Why, InfractionCount);
	} else { // Reason being: they are a dumb [REDACTED]
		The1984->Send("A negative action was performed by <@%llu>, Reason being: %s. This was infraction #%i for them.", User, Why, InfractionCount);
	}
	
	return;
}

void OnClientReady(User* DzUser) {
	printf("Ready, logged in as %s", DzUser->DisplayName);
	return;
}

void OnMessageCreate(Message* DzMessage) {
	if (DzMessage->Author->Id == 1120861232916877342)
		return;

	if (strstr(DzMessage->Content, "-p") == DzMessage->Content) { // purge messages
		if (DzMessage->Author->Id != 131545026105835520)
			return;
		DzMessage->Channel->BulkDelete(100);
	}

	if (DzMessage->Channel->Id == 1120856168265433129) { // report city
		if (strstr(DzMessage->Content, "https://steamcommunity.com/profiles/765") == DzMessage->Content ||
			strstr(DzMessage->Content, "https://steamcommunity.com/id/") == DzMessage->Content
		) {

			Table* Cheaters = DzBase->GetTable("DzCheaters");
			
			int ReportCount = 0;
			Entry* Him = Cheaters->GetEntryByPkPtr((char*)DzMessage->Content);
			if (!Him) {
				Him = Cheaters->CreateEntry((char*)DzMessage->Content);
				Him->Update<int>("CReportCnt", &ReportCount);
			} else {
				ReportCount = Him->Get<int>("CReportCnt");
				ReportCount++;
				Him->Update<int>("CReportCnt", &ReportCount);
			}
		} else {
			if (strstr(DzMessage->Content, "https://steamcommunity.com") == DzMessage->Content) {
				DzMessage->React(L'👎');
				Reprimand(DzMessage->Author->Id, "Sent a wrong steam link");
			} else if (strstr(DzMessage->Content, "http://") == DzMessage->Content) {
				DzMessage->Delete();
				Channel* BotCommands = DzClient->Channels.At(1120877166603812944);
				BotCommands->Send("Hey <@%llu>, we use encryption in this server.", DzMessage->Author->Id);
				Reprimand(DzMessage->Author->Id, "Decided to be insecure");
			} else {
				DzMessage->Delete();
				Reprimand(DzMessage->Author->Id, "Sent a non-steam link in cheater db");
			}
		}
	}

	if (DzMessage->Channel->Id == 1121683738447855646) { // verify channel
		if (strstr(DzMessage->Content, "-v ") == DzMessage->Content) {
			if (DzMessage->Member->Roles.At(1120853731592908820) == NULL)
				return;

			u64 UserId;
			sscanf(DzMessage->Content, "-v <@%llu>", &UserId);

			Member* RequestMember = DzClient->Guild.At(216726067770163200)->Members.At(UserId);
			Role* Verified = DzClient->Guild.At(216726067770163200)->Roles.At(1120853414574825533);
			Role* PendingVerify = DzClient->Guild.At(216726067770163200)->Roles.At(1121684845924470875);
			RequestMember->Roles.Putback(Verified);
			RequestMember->Roles.Delete(PendingVerify);
		} else {
			if (DzMessage->Member->Roles.At(1120853731592908820) == NULL) // they allow it for staff members
				return;

			if (strstr(DzMessage->Content, "https://steamcommunity.com") != DzMessage->Content) {
				DzMessage->Delete();
				Channel* Doghouse = DzClient->Channels.At(1120855758867812443);
				Role* BadNews = DzClient->Guild.At(216726067770163200)->Roles.At(1120855758867812443);
				DzMessage->Member->Roles.Putback(BadNews);
				Doghouse->Send("Hey, <@%llu>, let's only send steam links, okay?", DzMessage->Author->Id);
				Reprimand(DzMessage->Author->Id, "Sent a non-steam link in get verified");
			} else { // are they a dirty scumbag?
				Table* Cheaters = DzBase->GetTable("DzCheaters");
				Entry* MaybeCheating = Cheaters->GetEntryByPkPtr((void*)DzMessage->Content);	
				if (MaybeCheating != NULL) {
					// definintely cheating
					Role* CHEATER = DzClient->Guild.At(216726067770163200)->Roles.At(1120853550432534611);
					DzMessage->Member->Roles.Putback(CHEATER); // pethidic scum
				} else {
					// he's clean!
					Role* PendingVerify = DzClient->Guild.At(216726067770163200)->Roles.At(1121684845924470875);
					DzMessage->Member->Roles.Putback(PendingVerify);
				}
			}
		}
	}
	
	if (DzMessage->Channel->Id == 1120854645573689486 || // sanitizer
		DzMessage->Channel->Id == 1121672203038105671 ||
		DzMessage->Channel->Id == 1120855030774386769 ||
		DzMessage->Channel->Id == 1121679676763557990 ||
		DzMessage->Channel->Id == 1120861232916877342
	) {
		DzMessage->Delete();
		Reprimand(DzMessage->Author->Id, "Sent a message in an illegal channel");
	}
}

void OnMessageUpdate(Message* DzMessage) {
	if (DzMessage->Channel->Id == 1120856168265433129) {
		if (strstr(DzMessage->Content, "https://steamcommunity.com") == DzMessage->Content)
			return;
		DzMessage->Delete();
		Reprimand(DzMessage->Author->Id, "Edited a cheater db message");
	}
}

void OnGuildMemberAdd(Member* DzMember) {
	Channel* WelcomeChannel = DzClient->Channels.At(1120854645573689486);
	WelcomeChannel->Send("Welcome to 'CS2 // DangerZone', <@%llu>! Please check out <#1120855030774386769> and <#1120854629735993458>! Enjoy your stay.", DzMember->UserPtr->Id);
	Role* Community = DzClient->Guild.At(216726067770163200)->Roles.At(1120853191576277074);
	DzMember->Roles.Putback(Community);
	return;
}
