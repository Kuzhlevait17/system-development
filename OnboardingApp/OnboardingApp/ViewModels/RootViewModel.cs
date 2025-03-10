using Stylet;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace OnboardingApp.ViewModels
{
    public class RootViewModel : Conductor<IScreen>.Collection.OneActive, IDisposable
    {
        private readonly EmployersViewModel _employersViewModel;
        private readonly KnowlegeBaseViewModel _knowlegeBaseViewModel;
        private readonly MainMenuViewModel _mainMenuViewModel;
        private readonly OfficeMapViewModel _officeMapViewModel;
        private readonly TasksViewModel _tasksViewModel;



        private string _text = "";
        public string Text
        {
            get => _text;
            set
            {
                _text = value;
                OnPropertyChanged(nameof(Text));
            }
        }

        public RootViewModel(EmployersViewModel employersViewModel,
            KnowlegeBaseViewModel knowlegeBaseViewModel,
            MainMenuViewModel mainMenuViewModel,
            OfficeMapViewModel officeMapViewModel,
            TasksViewModel tasksViewModel)
        {
            _employersViewModel = employersViewModel;
            _knowlegeBaseViewModel = knowlegeBaseViewModel;
            _mainMenuViewModel = mainMenuViewModel;
            _officeMapViewModel = officeMapViewModel;
            _tasksViewModel = tasksViewModel;

            ActiveItem = mainMenuViewModel;

        }

        public void OpenMap()
        {
            ActiveItem = _officeMapViewModel;
        }

        public void SetTextCommand()
        {
            Text = "Дата и время: " + DateTime.Now.ToString();
        }


        public void Dispose()
        {
            throw new NotImplementedException();
        }
    }
}
