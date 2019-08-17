$(function () {
  var data = [
    {
      idx: 1,
      title: 'Load Core',
      content: 'Load your core by clicking first tab, scroll down until desired Core. We will use Nestopia for now. Do not forget; Content must be compatible with Core.'
    },
    {
      idx: 2,
      title: 'Load Content',
      content: 'After selecting Core, click Run. After RetroArch opens, click Add Content and select your compatible ROM.'
    },
    {
      idx: 3,
      title: 'Load Content',
      content: 'Select Load Content from the menu, and then enter Start Directory. You will see your Content, select it then select the Core'
    },
    {
      idx: 4,
      title: 'Cleanup Storage',
      content: 'The trash can erases your existing configuration and presets.'
    },
    {
      idx: 5,
      title: 'Quick Menu',
      content: 'If you click on the three line icons, the Quick Menu will open here as in RetroArch.'
    }
  ];
  
  var template =
    '<div class="slides" data-active-slide="1">' +
      '{{#slides}}' +
        '<article class="step" data-slide="{{idx}}">' +
          '<header>{{title}}</header>' +
          '<div class="content">{{content}}</div>' +
        '</article>' +
      '{{/slides}}' +
    '</div>';
  
  $('.how-to-guide').append(Mustache.render(template, { slides: data }));
  
  var slidesCount = data.length;
  var currentIdx = 1;
  
  $('body')
    .off('click', '.js-nav:not(.disabled)')
    .on('click', '.js-nav:not(.disabled)', function (event) {
      var button = $(event.currentTarget).blur();
      var container = button.parents('.js-container');
      
      var newIdx = currentIdx + (button.attr('data-nav') === 'prev' ? -1 : 1);
      container
        .find('.slides').attr('data-active-slide', newIdx).end()
        .find('.js-slide-no').html(newIdx);
      
      /* enable/disable nav buttons */  
      container.find('.js-nav').removeClass('disabled');
      if (newIdx <= 1) {
        container.find('.js-nav[data-nav="prev"]').addClass('disabled');
      } else if (newIdx >= slidesCount) {
        container.find('.js-nav[data-nav="next"]').addClass('disabled');
      }
      
      currentIdx = newIdx;
      
      return true;
    });
});