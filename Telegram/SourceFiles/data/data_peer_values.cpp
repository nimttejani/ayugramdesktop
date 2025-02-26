/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_peer_values.h"

#include "lang/lang_keys.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_user.h"
#include "data/data_changes.h"
#include "data/data_forum_topic.h"
#include "data/data_session.h"
#include "data/data_message_reactions.h"
#include "main/main_session.h"
#include "main/main_account.h"
#include "main/main_app_config.h"
#include "ui/image/image_prepare.h"
#include "base/unixtime.h"

// AyuGram includes
#include "ayu/ayu_settings.h"


namespace Data {
namespace {

constexpr auto kMinOnlineChangeTimeout = crl::time(1000);
constexpr auto kMaxOnlineChangeTimeout = 86400 * crl::time(1000);
constexpr auto kSecondsInDay = 86400;

int OnlinePhraseChangeInSeconds(TimeId online, TimeId now) {
	if (online <= 0) {
		if (-online > now) {
			return (-online - now);
		}
		return std::numeric_limits<TimeId>::max();
	}
	if (online > now) {
		return online - now;
	}
	const auto minutes = (now - online) / 60;
	if (minutes < 60) {
		return (minutes + 1) * 60 - (now - online);
	}
	const auto hours = (now - online) / 3600;
	if (hours < 12) {
		return (hours + 1) * 3600 - (now - online);
	}
	const auto nowFull = base::unixtime::parse(now);
	const auto tomorrow = nowFull.date().addDays(1).startOfDay();
	return std::max(static_cast<TimeId>(nowFull.secsTo(tomorrow)), 0);
}

std::optional<QString> OnlineTextSpecial(not_null<UserData*> user) {
	if (user->isNotificationsUser()) {
		return tr::lng_status_service_notifications(tr::now);
	} else if (user->isSupport()) {
		return tr::lng_status_support(tr::now);
	} else if (user->isBot()) {
		return tr::lng_status_bot(tr::now);
	} else if (user->isServiceUser()) {
		return tr::lng_status_support(tr::now);
	}
	return std::nullopt;
}

std::optional<QString> OnlineTextCommon(TimeId online, TimeId now) {
	if (online <= 0) {
		switch (online) {
		case 0:
		case -1: return tr::lng_status_offline(tr::now);
		case -2: return tr::lng_status_recently(tr::now);
		case -3: return tr::lng_status_last_week(tr::now);
		case -4: return tr::lng_status_last_month(tr::now);
		}
		return (-online > now)
			? tr::lng_status_online(tr::now)
			: tr::lng_status_recently(tr::now);
	} else if (online > now) {
		return tr::lng_status_online(tr::now);
	}
	return std::nullopt;
}

} // namespace

inline auto AdminRightsValue(not_null<ChannelData*> channel) {
	return channel->adminRightsValue();
}

inline auto AdminRightsValue(
		not_null<ChannelData*> channel,
		ChatAdminRights mask) {
	return FlagsValueWithMask(AdminRightsValue(channel), mask);
}

inline auto AdminRightValue(
		not_null<ChannelData*> channel,
		ChatAdminRight flag) {
	return SingleFlagValue(AdminRightsValue(channel), flag);
}

inline auto AdminRightsValue(not_null<ChatData*> chat) {
	return chat->adminRightsValue();
}

inline auto AdminRightsValue(
		not_null<ChatData*> chat,
		ChatAdminRights mask) {
	return FlagsValueWithMask(AdminRightsValue(chat), mask);
}

inline auto AdminRightValue(
		not_null<ChatData*> chat,
		ChatAdminRight flag) {
	return SingleFlagValue(AdminRightsValue(chat), flag);
}

inline auto RestrictionsValue(not_null<ChannelData*> channel) {
	return channel->restrictionsValue();
}

inline auto RestrictionsValue(
		not_null<ChannelData*> channel,
		ChatRestrictions mask) {
	return FlagsValueWithMask(RestrictionsValue(channel), mask);
}

inline auto RestrictionValue(
		not_null<ChannelData*> channel,
		ChatRestriction flag) {
	return SingleFlagValue(RestrictionsValue(channel), flag);
}

inline auto DefaultRestrictionsValue(not_null<ChannelData*> channel) {
	return channel->defaultRestrictionsValue();
}

inline auto DefaultRestrictionsValue(
		not_null<ChannelData*> channel,
		ChatRestrictions mask) {
	return FlagsValueWithMask(DefaultRestrictionsValue(channel), mask);
}

inline auto DefaultRestrictionValue(
		not_null<ChannelData*> channel,
		ChatRestriction flag) {
	return SingleFlagValue(DefaultRestrictionsValue(channel), flag);
}

inline auto DefaultRestrictionsValue(not_null<ChatData*> chat) {
	return chat->defaultRestrictionsValue();
}

inline auto DefaultRestrictionsValue(
		not_null<ChatData*> chat,
		ChatRestrictions mask) {
	return FlagsValueWithMask(DefaultRestrictionsValue(chat), mask);
}

inline auto DefaultRestrictionValue(
		not_null<ChatData*> chat,
		ChatRestriction flag) {
	return SingleFlagValue(DefaultRestrictionsValue(chat), flag);
}

// Duplicated in CanSendAnyOf().
[[nodiscard]] rpl::producer<bool> CanSendAnyOfValue(
		not_null<Thread*> thread,
		ChatRestrictions rights,
		bool forbidInForums) {
	if (const auto topic = thread->asTopic()) {
		using Flag = ChannelDataFlag;
		const auto mask = Flag()
			| Flag::Left
			| Flag::JoinToWrite
			| Flag::HasLink
			| Flag::Forbidden
			| Flag::Creator;
		const auto channel = topic->channel();
		return rpl::combine(
			PeerFlagsValue(channel.get(), mask),
			RestrictionsValue(channel, rights),
			DefaultRestrictionsValue(channel, rights),
			AdminRightsValue(channel, ChatAdminRight::ManageTopics),
			topic->session().changes().topicFlagsValue(
				topic,
				TopicUpdate::Flag::Closed),
			[=](
					ChannelDataFlags flags,
					ChatRestrictions sendRestriction,
					ChatRestrictions defaultSendRestriction,
					auto,
					auto) {
				const auto notAmInFlags = Flag::Left | Flag::Forbidden;
				const auto allowed = !(flags & notAmInFlags)
					|| ((flags & Flag::HasLink)
						&& !(flags & Flag::JoinToWrite));
				return allowed
					&& ((flags & Flag::Creator)
						|| (!sendRestriction && !defaultSendRestriction))
					&& (!topic->closed() || topic->canToggleClosed());
			});
	}
	return CanSendAnyOfValue(thread->peer(), rights, forbidInForums);
}

// Duplicated in CanSendAnyOf().
[[nodiscard]] rpl::producer<bool> CanSendAnyOfValue(
		not_null<PeerData*> peer,
		ChatRestrictions rights,
		bool forbidInForums) {
	if (const auto user = peer->asUser()) {
		if (user->isRepliesChat()) {
			return rpl::single(false);
		}
		using namespace rpl::mappers;
		const auto other = rights & ~(ChatRestriction::SendVoiceMessages
			| ChatRestriction::SendVideoMessages);
		if (other) {
			return PeerFlagValue(user, UserDataFlag::Deleted)
				| rpl::map(!_1);
		}
		const auto mask = UserDataFlag::Deleted
			| UserDataFlag::VoiceMessagesForbidden;
		return PeerFlagsValue(user, mask) | rpl::map(!_1);
	} else if (const auto chat = peer->asChat()) {
		const auto mask = ChatDataFlag()
			| ChatDataFlag::Deactivated
			| ChatDataFlag::Forbidden
			| ChatDataFlag::Left
			| ChatDataFlag::Creator;
		return rpl::combine(
			PeerFlagsValue(chat, mask),
			AdminRightsValue(chat),
			DefaultRestrictionsValue(chat, rights),
			[rights](
				ChatDataFlags flags,
				Data::Flags<ChatAdminRights>::Change adminRights,
				ChatRestrictions defaultSendRestrictions) {
			const auto amOutFlags = ChatDataFlag()
				| ChatDataFlag::Deactivated
				| ChatDataFlag::Forbidden
				| ChatDataFlag::Left;
		return !(flags & amOutFlags)
			&& ((flags & ChatDataFlag::Creator)
				|| (adminRights.value != ChatAdminRights(0))
				|| (rights & ~defaultSendRestrictions));
		});
	} else if (const auto channel = peer->asChannel()) {
		using Flag = ChannelDataFlag;
		const auto mask = Flag()
			| Flag::Left
			| Flag::Forum
			| Flag::JoinToWrite
			| Flag::HasLink
			| Flag::Forbidden
			| Flag::Creator
			| Flag::Broadcast;
		return rpl::combine(
			PeerFlagsValue(channel, mask),
			AdminRightValue(
				channel,
				ChatAdminRight::PostMessages),
			RestrictionsValue(channel, rights),
			DefaultRestrictionsValue(channel, rights),
			[=](
					ChannelDataFlags flags,
					bool postMessagesRight,
					ChatRestrictions sendRestriction,
					ChatRestrictions defaultSendRestriction) {
				const auto notAmInFlags = Flag::Left | Flag::Forbidden;
				const auto forumRestriction = forbidInForums
					&& (flags & Flag::Forum);
				const auto allowed = !(flags & notAmInFlags)
					|| ((flags & Flag::HasLink)
						&& !(flags & Flag::JoinToWrite));
				const auto restricted = sendRestriction
					| defaultSendRestriction;
				return allowed
					&& !forumRestriction
					&& (postMessagesRight
						|| (flags & Flag::Creator)
						|| (!(flags & Flag::Broadcast)
							&& (rights & ~restricted)));
			});
	}
	Unexpected("Peer type in Data::CanSendAnyOfValue.");
}

// This is duplicated in PeerData::canPinMessages().
rpl::producer<bool> CanPinMessagesValue(not_null<PeerData*> peer) {
	using namespace rpl::mappers;
	if (const auto user = peer->asUser()) {
		return PeerFlagsValue(
			user,
			UserDataFlag::CanPinMessages
		) | rpl::map(_1 != UserDataFlag(0));
	} else if (const auto chat = peer->asChat()) {
		const auto mask = 0
			| ChatDataFlag::Deactivated
			| ChatDataFlag::Forbidden
			| ChatDataFlag::Left
			| ChatDataFlag::Creator;
		return rpl::combine(
			PeerFlagsValue(chat, mask),
			AdminRightValue(chat, ChatAdminRight::PinMessages),
			DefaultRestrictionValue(chat, ChatRestriction::PinMessages),
		[](
				ChatDataFlags flags,
				bool adminRightAllows,
				bool defaultRestriction) {
			const auto amOutFlags = 0
				| ChatDataFlag::Deactivated
				| ChatDataFlag::Forbidden
				| ChatDataFlag::Left;
			return !(flags & amOutFlags)
				&& ((flags & ChatDataFlag::Creator)
					|| adminRightAllows
					|| !defaultRestriction);
		});
	} else if (const auto megagroup = peer->asMegagroup()) {
		if (megagroup->amCreator()) {
			return rpl::single(true);
		}
		return rpl::combine(
			AdminRightValue(megagroup, ChatAdminRight::PinMessages),
			DefaultRestrictionValue(megagroup, ChatRestriction::PinMessages),
			PeerFlagsValue(
				megagroup,
				ChannelDataFlag::Username | ChannelDataFlag::Location),
			megagroup->restrictionsValue()
		) | rpl::map([=](
				bool adminRightAllows,
				bool defaultRestriction,
				ChannelDataFlags usernameOrLocation,
				Data::Flags<ChatRestrictions>::Change restrictions) {
			return adminRightAllows
				|| (!usernameOrLocation
					&& !defaultRestriction
					&& !(restrictions.value & ChatRestriction::PinMessages));
		});
	} else if (const auto channel = peer->asChannel()) {
		if (channel->amCreator()) {
			return rpl::single(true);
		}
		return AdminRightValue(channel, ChatAdminRight::EditMessages);
	}
	Unexpected("Peer type in CanPinMessagesValue.");
}

rpl::producer<bool> CanManageGroupCallValue(not_null<PeerData*> peer) {
	const auto flag = ChatAdminRight::ManageCall;
	if (const auto chat = peer->asChat()) {
		return chat->amCreator()
			? (rpl::single(true) | rpl::type_erased())
			: AdminRightValue(chat, flag);
	} else if (const auto channel = peer->asChannel()) {
		return channel->amCreator()
			? (rpl::single(true) | rpl::type_erased())
			: AdminRightValue(channel, flag);
	}
	return rpl::single(false);
}

rpl::producer<bool> PeerPremiumValue(not_null<PeerData*> peer) {
	const auto user = peer->asUser();
	if (!user) {
		return rpl::single(false);
	}
	return user->flagsValue(
	) | rpl::filter([=](UserData::Flags::Change change) {
		return (change.diff & UserDataFlag::Premium);
	}) | rpl::map([=] {
		return user->isPremium();
	});
}

rpl::producer<bool> AmPremiumValue(not_null<Main::Session*> session) {
	auto settings = &AyuSettings::getInstance();
	if (settings->localPremium) {
		return rpl::single(true);
	}

	return PeerPremiumValue(session->user());
}

TimeId SortByOnlineValue(not_null<UserData*> user, TimeId now) {
	if (user->isServiceUser() || user->isBot()) {
		return -1;
	}
	const auto online = user->onlineTill;
	if (online <= 0) {
		switch (online) {
		case 0:
		case -1: return online;

		case -2: {
			return now - 3 * kSecondsInDay;
		} break;

		case -3: {
			return now - 7 * kSecondsInDay;
		} break;

		case -4: {
			return now - 30 * kSecondsInDay;
		} break;
		}
		return -online;
	}
	return online;
}

crl::time OnlineChangeTimeout(TimeId online, TimeId now) {
	const auto result = OnlinePhraseChangeInSeconds(online, now);
	Assert(result >= 0);
	return std::clamp(
		result * crl::time(1000),
		kMinOnlineChangeTimeout,
		kMaxOnlineChangeTimeout);
}

crl::time OnlineChangeTimeout(not_null<UserData*> user, TimeId now) {
	if (user->isServiceUser() || user->isBot()) {
		return kMaxOnlineChangeTimeout;
	}
	return OnlineChangeTimeout(user->onlineTill, now);
}

QString OnlineText(TimeId online, TimeId now) {
	if (const auto common = OnlineTextCommon(online, now)) {
		return *common;
	}
	const auto minutes = (now - online) / 60;
	if (!minutes) {
		return tr::lng_status_lastseen_now(tr::now);
	} else if (minutes < 60) {
		return tr::lng_status_lastseen_minutes(tr::now, lt_count, minutes);
	}
	const auto hours = (now - online) / 3600;
	if (hours < 12) {
		return tr::lng_status_lastseen_hours(tr::now, lt_count, hours);
	}
	const auto onlineFull = base::unixtime::parse(online);
	const auto nowFull = base::unixtime::parse(now);
	const auto locale = QLocale();
	if (onlineFull.date() == nowFull.date()) {
		const auto onlineTime = locale.toString(onlineFull.time(), QLocale::ShortFormat);
		return tr::lng_status_lastseen_today(tr::now, lt_time, onlineTime);
	} else if (onlineFull.date().addDays(1) == nowFull.date()) {
		const auto onlineTime = locale.toString(onlineFull.time(), QLocale::ShortFormat);
		return tr::lng_status_lastseen_yesterday(tr::now, lt_time, onlineTime);
	}
	const auto date = locale.toString(onlineFull.date(), QLocale::ShortFormat);
	return tr::lng_status_lastseen_date(tr::now, lt_date, date);
}

QString OnlineText(not_null<UserData*> user, TimeId now) {
	if (const auto special = OnlineTextSpecial(user)) {
		return *special;
	}
	return OnlineText(user->onlineTill, now);
}

QString OnlineTextFull(not_null<UserData*> user, TimeId now) {
	if (const auto special = OnlineTextSpecial(user)) {
		return *special;
	} else if (const auto common = OnlineTextCommon(user->onlineTill, now)) {
		return *common;
	}
	const auto onlineFull = base::unixtime::parse(user->onlineTill);
	const auto nowFull = base::unixtime::parse(now);
	const auto locale = QLocale();
	if (onlineFull.date() == nowFull.date()) {
		const auto onlineTime = locale.toString(onlineFull.time(), QLocale::ShortFormat);
		return tr::lng_status_lastseen_today(tr::now, lt_time, onlineTime);
	} else if (onlineFull.date().addDays(1) == nowFull.date()) {
		const auto onlineTime = locale.toString(onlineFull.time(), QLocale::ShortFormat);
		return tr::lng_status_lastseen_yesterday(tr::now, lt_time, onlineTime);
	}
	const auto date = locale.toString(onlineFull.date(), QLocale::ShortFormat);
	const auto time = locale.toString(onlineFull.time(), QLocale::ShortFormat);
	return tr::lng_status_lastseen_date_time(tr::now, lt_date, date, lt_time, time);
}

bool OnlineTextActive(TimeId online, TimeId now) {
	if (online <= 0) {
		switch (online) {
		case 0:
		case -1:
		case -2:
		case -3:
		case -4: return false;
		}
		return (-online > now);
	}
	return (online > now);
}

bool OnlineTextActive(not_null<UserData*> user, TimeId now) {
	if (user->isServiceUser() || user->isBot()) {
		return false;
	}
	return OnlineTextActive(user->onlineTill, now);
}

bool IsUserOnline(not_null<UserData*> user, TimeId now) {
	if (!now) {
		now = base::unixtime::now();
	}
	return OnlineTextActive(user, now);
}

bool ChannelHasActiveCall(not_null<ChannelData*> channel) {
	return (channel->flags() & ChannelDataFlag::CallNotEmpty);
}

rpl::producer<QImage> PeerUserpicImageValue(
		not_null<PeerData*> peer,
		int size,
		std::optional<int> radius) {
	return [=](auto consumer) {
		auto result = rpl::lifetime();
		struct State {
			Ui::PeerUserpicView view;
			rpl::lifetime waiting;
			InMemoryKey key = {};
			bool empty = true;
			Fn<void()> push;
		};
		const auto state = result.make_state<State>();
		state->push = [=] {
			const auto key = peer->userpicUniqueKey(state->view);
			const auto loading = Ui::PeerUserpicLoading(state->view);

			if (loading && !state->waiting) {
				peer->session().downloaderTaskFinished(
				) | rpl::start_with_next(state->push, state->waiting);
			} else if (!loading && state->waiting) {
				state->waiting.destroy();
			}

			if (!state->empty && (loading || key == state->key)) {
				return;
			}
			state->key = key;
			state->empty = false;
			consumer.put_next(peer->generateUserpicImage(
				state->view,
				size,
				radius));
		};
		peer->session().changes().peerFlagsValue(
			peer,
			PeerUpdate::Flag::Photo
		) | rpl::start_with_next(state->push, result);
		return result;
	};
}

const AllowedReactions &PeerAllowedReactions(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return chat->allowedReactions();
	} else if (const auto channel = peer->asChannel()) {
		return channel->allowedReactions();
	} else {
		static const auto result = AllowedReactions{
			.type = AllowedReactionsType::All,
		};
		return result;
	}
}

 rpl::producer<AllowedReactions> PeerAllowedReactionsValue(
		not_null<PeerData*> peer) {
	return peer->session().changes().peerFlagsValue(
		peer,
		Data::PeerUpdate::Flag::Reactions
	) | rpl::map([=]{
		return PeerAllowedReactions(peer);
	});
}

int UniqueReactionsLimit(not_null<Main::AppConfig*> config) {
	return config->get<int>("reactions_uniq_max", 11);
}

int UniqueReactionsLimit(not_null<PeerData*> peer) {
	return UniqueReactionsLimit(&peer->session().account().appConfig());
}

rpl::producer<int> UniqueReactionsLimitValue(
		not_null<PeerData*> peer) {
	const auto config = &peer->session().account().appConfig();
	return config->value(
	) | rpl::map([=] {
		return UniqueReactionsLimit(config);
	}) | rpl::distinct_until_changed();
}

} // namespace Data
