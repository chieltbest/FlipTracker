#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <gtkmm.h>

namespace sigc {
	SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

#pragma GCC diagnostic warning "-Wdeprecated-declarations"

#include <libnotify/notify.h>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#define PAUSED_ICON "gtk-media-play"
#define COUNTING_ICON "gtk-apply"

#define RAISE_AMOUNT 25

class Timer {
public:
	enum State : unsigned {
		PENDING,
		STARTED,
		ELAPSED
	};

	Timer(State start = PENDING)
		: state{start},
		  startTime{std::chrono::system_clock::now()} {
	}

	Timer(const Timer &copy)
		: state{copy.state},
		  startTime{copy.startTime} {
	}

	std::chrono::system_clock::duration getElapsed() const {
		return state != PENDING
		       ? std::chrono::system_clock::now() - startTime
		       : std::chrono::nanoseconds{0};
	}

	float getPercentage(std::chrono::system_clock::duration total) const {
		float percentage = float(getElapsed().count()) / total.count();
		return percentage > 1
		       ? 1
		       : percentage;
	}

	void start() {
		startTime = std::chrono::system_clock::now();
		state = STARTED;
	}

	void elapse() {
		state = ELAPSED;
	}

	bool isRunning() {
		return state == STARTED;
	}

	State getState() {
		return state;
	}

	bool operator<(const Timer &rhs) const {
		return getElapsed().count() < rhs.getElapsed().count();
	}

	bool operator==(const Timer &rhs) const {
		return getElapsed().count() == rhs.getElapsed().count();
	}

	bool operator>(const Timer &rhs) const {
		return getElapsed().count() > rhs.getElapsed().count();
	}

	friend std::ostream &operator<<(std::ostream &lhs, const Timer &rhs) {
		return lhs << rhs.state << " " << rhs.startTime.time_since_epoch().count();
	}

	friend std::istream &operator>>(std::istream &lhs, Timer &rhs) {
		unsigned long long epochTime;
		unsigned state;
		lhs >> state >> epochTime;
		rhs.state = State(state);
		rhs.startTime =
			std::chrono::system_clock::time_point{std::chrono::system_clock::duration{epochTime}};
		return lhs;
	}

private:
	State state;
	std::chrono::system_clock::time_point startTime;
};

class FlipColumns : public Gtk::TreeModel::ColumnRecord {
public:
	FlipColumns() {
		add(okButt);
		add(selling);
		add(name);
		add(value);
		add(progress);
		add(rule);
	}

	Gtk::TreeModelColumn<Glib::ustring> okButt;
	Gtk::TreeModelColumn<bool> selling;
	Gtk::TreeModelColumn<Glib::ustring> name;
	Gtk::TreeModelColumn<unsigned int> value;
	Gtk::TreeModelColumn<Timer> progress;
	Gtk::TreeModelColumn<unsigned int> rule;
};

std::tuple<int, std::string> decompose(Glib::ustring name) {
	std::stringstream inString{name}, outstring;
	int items = 1;
	if ((inString >> items).fail()) {
		items = 1;
	}
	outstring << inString.rdbuf();

	return std::tuple<int, std::string>{items, outstring.str()};
};

std::string stripLeadingWhitespace(std::string string) {
	unsigned long pos = string.find_first_not_of(' ');
	if (pos < string.size()) {
		return string.substr(pos);
	} else {
		return string;
	}
}

std::string stripTrailingWhitespace(std::string string) {
	unsigned long pos = string.find_last_not_of(' ');
	return string.substr(0, pos + 1);
}

Gtk::TreeModel::iterator addRow(Glib::RefPtr<Gtk::ListStore> &model, FlipColumns &cols,
                                bool sellingVal, Glib::ustring nameVal, unsigned valueVal,
                                Timer progressVal = {}, unsigned ruleVal = 0,
                                Gtk::TreeModel::iterator after = {}) {
	Gtk::TreeModel::Row row;
	if (after) {
		row = (*model->insert_after(after));
	} else {
		row = (*model->append());
	}
	row[cols.okButt] = progressVal.isRunning()
	                   ? COUNTING_ICON
	                   : PAUSED_ICON;
	row[cols.selling] = sellingVal;
	row[cols.name] = nameVal;
	row[cols.value] = valueVal;
	row[cols.progress] = progressVal;
	row[cols.rule] = ruleVal;

	return row;
}

Gtk::TreeModel::iterator addItemSell(Glib::RefPtr<Gtk::ListStore> &model, FlipColumns &cols,
                                     Glib::ustring item, Gtk::TreeModel::iterator after = {}) {
	for (auto it : model->children()) {
		if (it->get_value(cols.selling)) {
			std::tuple<int, std::string> rowItem = decompose(it.get_value(cols.name));
			if (stripTrailingWhitespace(stripLeadingWhitespace(std::get<1>(rowItem)))
			    == stripTrailingWhitespace(stripLeadingWhitespace(item))) {
				std::stringstream newName;
				newName << std::get<0>(rowItem) + 1 << " " << item;
				it->set_value(cols.name, {newName.str()});
				return it;
			}
		}
	}
	return addRow(model, cols, true, item, 0, {Timer::PENDING}, 0, after);
}

template<typename TreeModel, typename Columns>
void saveToFile(TreeModel &treeModel, Columns &columns) {
	mkdir(".fliptracker1/", 0777);
	std::ofstream outFlips{".fliptracker1/flips.txt"};
	for (auto col : treeModel->children()) {
		std::string name = col.get_value(columns.name);
		outFlips << "\"" << name << "\" " << col.get_value(columns.selling) << " "
		         << col.get_value(columns.value) << " " << col.get_value(columns.progress)
		         << " " << col.get_value(columns.rule) << std::endl;
	}
}

class CellRendererButton : public Gtk::CellRendererPixbuf {
public:
	CellRendererButton(Glib::RefPtr<Gtk::ListStore> &tree, Gtk::TreeView *treeView,
	                   FlipColumns &columns, std::chrono::system_clock::duration &raiseTime)
		: tree(tree),
		  treeView{treeView},
		  columns(columns),
		  raiseTime(raiseTime) {
		property_sensitive() = true;
		property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
	}

protected:
	virtual bool activate_vfunc(GdkEvent *event, Gtk::Widget &widget, const Glib::ustring &path,
	                            const Gdk::Rectangle &background_area,
	                            const Gdk::Rectangle &cell_area,
	                            Gtk::CellRendererState flags) override {
		Gtk::TreePath treePath{path};
		Gtk::TreeModel::iterator iter = tree->get_iter(path);
		if (!iter->get_value(columns.progress).isRunning()) {
			iter->set_value(columns.okButt, {COUNTING_ICON});
		} else {
			Glib::ustring newName = decrementItems(iter->get_value(columns.name));
			if (newName == "") {
				if (!iter->get_value(columns.selling)) {
					addItemSell(tree, columns, iter->get_value(columns.name), iter);
				}

				tree->erase(iter);
				treeView->get_selection()->unselect(treePath);
			} else {
				iter->set_value(columns.name, newName);

				if (!iter->get_value(columns.selling)) {
					std::stringstream ss{newName}, name;

					int count;
					ss >> count;
					name << ss.rdbuf();

					addItemSell(tree, columns,
					            stripTrailingWhitespace(stripLeadingWhitespace(name.str())), iter);
				}
			}
		}
		iter->set_value(columns.progress, {Timer::STARTED});

		saveToFile(tree, columns);

		return true;
	}

private:
	Glib::ustring decrementItems(Glib::ustring &&itemName) {
		std::tuple<int, std::string> item = decompose(itemName);
		int items = std::get<0>(item);
		if (--items <= 0) {
			return "";
		} else {
			std::stringstream nameStream;
			std::string itemNameStrip{std::get<1>(item)};
			if (items > 1) {
				nameStream << items;
			} else {
				itemNameStrip = stripLeadingWhitespace(itemNameStrip);
			}
			itemNameStrip = stripTrailingWhitespace(itemNameStrip);
			nameStream << itemNameStrip;
			return nameStream.str();
		}
	}

	Glib::RefPtr<Gtk::ListStore> tree;
	Gtk::TreeView *treeView;
	FlipColumns &columns;
	std::chrono::system_clock::duration &raiseTime;
};

bool strEqualIgnoreCase(std::string &str1, std::string &str2) {
	if (str1.length() != str2.length()) {
		return false;
	}

	for (int i{0}; i < str1.length(); i++) {
		if (std::tolower(str1[i]) != std::tolower(str2[i])) {
			return false;
		}
	}
	return true;
}

bool strLessIgnoreCase(std::string &str1, std::string &str2) {
	for (int i{0}; i < str1.length() && i < str2.length(); i++) {
		if (std::tolower(str1[i]) < std::tolower(str2[i])) {
			return true;
		} else if (std::tolower(str1[i]) > std::tolower(str2[i])) {
			return false;
		}
	}
	return str1.length() < str2.length();
}

int main(int argc, char **argv) {
	// CONFIGURATION
	std::chrono::system_clock::duration raiseTime = std::chrono::minutes{30};

	auto app = Gtk::Application::create(argc, argv);
	notify_init("FlipTracker1");

	Gtk::Window window;
	window.set_default_size(400, 566);
	window.set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
	window.set_keep_above(true);
	window.set_title("FlipTracker1");

	Gtk::Box *box = Gtk::manage(new Gtk::Box{Gtk::ORIENTATION_VERTICAL});
	window.add(*box);

	Gtk::ScrolledWindow *scroll = Gtk::manage(new Gtk::ScrolledWindow{});
	scroll->property_hscrollbar_policy() = Gtk::POLICY_NEVER;
	box->pack_start(*scroll, true, true);

	FlipColumns columns;

	Glib::RefPtr<Gtk::ListStore> treeModel = Gtk::ListStore::create(columns);
	treeModel->set_sort_func(columns.name, [&](const Gtk::TreeModel::iterator &a,
	                                           const Gtk::TreeModel::iterator &b) {
		std::tuple<int, std::string> nameA = decompose(a->get_value(columns.name)),
			nameB = decompose(b->get_value(columns.name));
		std::string
			truncNameA = stripTrailingWhitespace(stripLeadingWhitespace(std::get<1>(nameA))),
			truncNameB = stripTrailingWhitespace(stripLeadingWhitespace(std::get<1>(nameB)));

		if (strLessIgnoreCase(truncNameA, truncNameB)) {
			return -1;
		} else if (strEqualIgnoreCase(truncNameA, truncNameB)) {
			if (std::get<0>(nameA) < std::get<0>(nameB)) {
				return -1;
			} else if (std::get<0>(nameA) > std::get<0>(nameB)) {
				return 1;
			}
			return 0;
		} else {
			return 1;
		}
	});
	treeModel->set_sort_func(columns.progress, [&](const Gtk::TreeModel::iterator &a,
	                                               const Gtk::TreeModel::iterator &b) {
		float timeA = a->get_value(columns.progress).getPercentage(raiseTime),
			timeB = b->get_value(columns.progress).getPercentage(raiseTime);

		if (timeB < timeA) {
			return -1;
		} else if (timeA == timeB) {
			return 0;
		} else {
			return 1;
		}
	});
	treeModel->signal_row_changed()
	         .connect([&](const Gtk::TreePath &path, const Gtk::TreeModel::iterator &iter) {
		         // TODO make this configurable
		         treeModel->set_sort_column(Gtk::TreeSortable::DEFAULT_UNSORTED_COLUMN_ID,
		                                    Gtk::SORT_ASCENDING);
	         });

	Gtk::TreeView *tree = Gtk::manage(new Gtk::TreeView{treeModel});
	tree->set_reorderable(true);
	tree->set_search_column(columns.name);
	tree->get_selection()->set_mode(Gtk::SELECTION_NONE);
	tree->signal_row_activated()
	    .connect([&](const Gtk::TreePath &path, Gtk::TreeViewColumn *column) {
		    tree->set_cursor(path, *column, true);
	    });
	tree->signal_drag_begin().connect([&](const Glib::RefPtr<Gdk::DragContext> &context) {
		treeModel->set_sort_column(Gtk::TreeSortable::DEFAULT_UNSORTED_COLUMN_ID,
		                           Gtk::SORT_ASCENDING);
	}, false);

	CellRendererButton okButtPix{treeModel, tree, columns, raiseTime};
	tree->append_column("", okButtPix);

	tree->append_column_editable("selling", columns.selling);
	tree->append_column_editable("name", columns.name);
	tree->append_column_editable("value", columns.value);

	Gtk::CellRendererProgress progressCell;
	tree->insert_column_with_data_func(-1, "progress", progressCell,
	                                   Gtk::TreeViewColumn::SlotCellData(
		                                   [&](Gtk::CellRenderer *renderer,
		                                       const Gtk::TreeIter &data) {
			                                   auto progRender =
				                                   static_cast<Gtk::CellRendererProgress *>(renderer);

			                                   float percentage = data->get_value(columns.progress)
			                                                          .getPercentage(raiseTime)
			                                                      * 100;
			                                   std::chrono::system_clock::duration remaining =
				                                   raiseTime
				                                   - data->get_value(columns.progress).getElapsed();
			                                   int secsRemain =
				                                   int(std::chrono::duration<float, std::milli>(
					                                   remaining).count()) / 1000;

			                                   if (secsRemain < 0) {
				                                   secsRemain = 0;
			                                   }

			                                   progRender->property_value() = percentage;

			                                   std::stringstream ss;
			                                   ss << secsRemain / 60 << ":" << std::setfill('0')
			                                      << std::setw(2) << secsRemain % 60;
			                                   progRender->property_text() = ss.str();

			                                   if (percentage < 100) {
				                                   tree->queue_draw();
			                                   } else if (data->get_value(columns.progress).isRunning()) {
				                                   // the counter has elapsed at this point
				                                   data->set_value(columns.okButt, {PAUSED_ICON});

				                                   if (data->get_value(columns.selling)) {
					                                   if (data->get_value(columns.value)
					                                       < RAISE_AMOUNT) {
						                                   data->set_value(columns.value,
						                                                   (unsigned int) 0);
					                                   } else {
						                                   data->set_value(columns.value,
						                                                   data->get_value(
							                                                   columns.value)
						                                                   + -RAISE_AMOUNT);
					                                   }
				                                   } else {
					                                   data->set_value(columns.value,
					                                                   data->get_value(
						                                                   columns.value)
					                                                   + RAISE_AMOUNT);
				                                   }

				                                   Timer tim{data->get_value(columns.progress)};
				                                   tim.elapse();
				                                   data->set_value(columns.progress, tim);
				                                   data->set_value(columns.rule,
				                                                   data->get_value(columns.rule)
				                                                   + 1);
				                                   tree->queue_draw();

				                                   // TODO make ping sound when counter finishes
				                                   NotifyNotification *note =
					                                   notify_notification_new("FlipTracker1",
					                                                           (data->get_value(
						                                                           columns.name)
					                                                            + " elapsed").c_str(),
					                                                           "gtk-apply");
				                                   GError *err = nullptr;
				                                   notify_notification_show(note, &err);

//				                                   saveToFile(treeModel, columns);
			                                   }
		                                   }));

	tree->append_column_editable("rule", columns.rule);

	tree->get_column(0)->add_attribute(okButtPix, "icon_name", columns.okButt);
	tree->get_column(0)->set_reorderable(true);
	tree->get_column(1)->set_sort_column(columns.selling);
	tree->get_column(1)->set_reorderable(true);
	tree->get_column(2)->set_sort_column(columns.name);
	tree->get_column(2)->set_reorderable(true);
	tree->get_column(3)->set_sort_column(columns.value);
	//	tree->get_column(3)->add_attribute(spinCell, "text", columns.value);
	tree->get_column(3)->set_reorderable(true);
	tree->get_column(4)->set_sort_column(columns.progress);
	tree->get_column(4)->set_reorderable(true);
	tree->get_column(4)->property_expand() = true;
	tree->get_column(5)->set_sort_column(columns.rule);
	tree->get_column(5)->set_reorderable(true);

	//	box->pack_start(*tree, true, true);
	scroll->add(*tree);

	Gtk::Box buttBox;
	Gtk::Button butt{Gtk::StockID{"gtk-add"}};
	butt.property_always_show_image() = true;

	buttBox.pack_start(butt, false, true);

	butt.signal_clicked().connect([&]() {
		addRow(treeModel, columns, false, "", 0, {Timer::PENDING}, 0);
	});

	box->pack_end(buttBox, false, true);
	box->show_all();

	{
		std::ifstream inFlips{".fliptracker1/flips.txt"};
		int lineNum = 1;
		while (inFlips) {
			std::string line;
			std::getline(inFlips, line);
			std::stringstream lineStream{line};

			std::string item = "";
			bool selling = 0;
			unsigned value = 0;
			Timer time{};
			unsigned rule = 0;
			if (line != "") {
				std::getline(std::getline(lineStream, item, '\"'), item, '\"');
				lineStream >> selling >> value >> time >> rule;
				addRow(treeModel, columns, selling, item, value, time, rule);
			}
			lineNum++;
		}
	}

	for (auto it : treeModel->children()) {
		if (it->get_value(columns.progress).getPercentage(raiseTime) >= 1
		    && it->get_value(columns.okButt) == COUNTING_ICON) {
			it->set_value(columns.okButt, {PAUSED_ICON});
		}
	}

	int exitCode = app->run(window);

	saveToFile(treeModel, columns);

	return exitCode;
}
